// ==========================================================================
// PROJECT: Space Invders - silent version
// HARDWARE: ESP32-WROOM Devkit v1
// CORE VERSION: 2.0.17 (Required?)
// This version strips the sound and 12s code.
// ==========================================================================


#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <driver/i2s.h>
#include <vector>
#include <Update.h>  // Fuer Web-OTA


// --- LED CONFIGURATION ---
///#define FASTLED_ESP32_S3_PIN 7
///#define FASTLED_RMT_MAX_CHANNELS 1
#include <FastLED.h>




// --------------------------------------------------------------------------
// 1. DEFINITIONS & DATA TYPES
// --------------------------------------------------------------------------
#define I2S_BCLK 4
#define I2S_LRC 5
#define I2S_DOUT 6

#define PIN_LED_DATA 2
#define MAX_LEDS 1200
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

#define PIN_BTN_BLUE 15
#define PIN_BTN_RED 16
#define PIN_BTN_GREEN 17
#define PIN_BTN_WHITE 18

#define CONFIG_VERSION 31
#define FRAME_DELAY 16  // 16ms = approx. 60 FPS
#define INPUT_BUFFER_MS 60
#define SAMPLE_RATE 44100

const int FIRE_COOLDOWN = 100;

struct ToneCmd {
  int freq;
  int duration;
};
typedef std::vector<ToneCmd> Melody;

enum SoundEvent {
  EVT_NONE = 0,
  EVT_START,
  EVT_WIN,
  EVT_LOSE,
  EVT_MISTAKE,
  EVT_HIT_SUCCESS,
  EVT_SHOT_BLUE,
  EVT_SHOT_RED,
  EVT_SHOT_GREEN,
  EVT_SHOT_WHITE
};

enum GameState {
  STATE_MENU,
  STATE_INTRO,
  STATE_PLAYING,
  STATE_BOSS_PLAYING,
  STATE_LEVEL_COMPLETED,
  STATE_GAME_FINISHED,
  STATE_BASE_DESTROYED,
  STATE_GAMEOVER
};

enum Boss2State { B2_MOVE,
                  B2_CHARGE,
                  B2_SHOOT };
enum Boss3State { B3_MOVE,
                  B3_PHASE_CHANGE,
                  B3_BURST,
                  B3_WAIT };

struct LevelConfig {
  int speed;
  int length;
  int bossType;
};
struct BossConfig {
  int moveSpeed;
  int shotSpeed;
  int hpPerLed;
  int shotFreq;
  int burstCount;
  int m1;
  int m2;
  int m3;
};
struct Enemy {
  int color;
  float pos;
};
struct BossSegment {
  int color;
  int hp;
  int maxHp;
  bool active;
  int originalIndex;
};
struct Shot {
  float position;
  int color;
};
struct BossProjectile {
  float pos;
  int color;
};


// -------------------------------------------------------------------------
// Manual prototypes to prevent Arduino from auto-generating broken ones
// -------------------------------------------------------------------------
CRGB hexToCRGB(String hex);
Melody parseSoundString(String data);
void melodyFromStr(Melody &m, String s);

void playSound(SoundEvent evt);
void playShotSound(int color);

CRGB getColor(int colorCode);
void drawCrispPixel(float pos, CRGB color);
void flashPixel(int pos);



// --------------------------------------------------------------------------
// 2. GLOBAL VARIABLES
// --------------------------------------------------------------------------
CRGB leds[MAX_LEDS];
Preferences preferences;
WebServer server(80);
///QueueHandle_t audioQueue;
///TaskHandle_t audioTaskHandle;

// Statistics
unsigned long stat_totalShots = 0;
unsigned long stat_totalKills = 0;
int stat_lastGameShots = 0;

// Default Sound Strings
const String DEF_SND_START = "523,80;659,80;784,80;1047,300";
const String DEF_SND_WIN = "523,80;659,80;784,80;1047,300;0,150;1047,60;1319,60";
const String DEF_SND_LOSE = "370,100;349,100;330,100;311,400";
const String DEF_SND_MISTAKE = "60,150";
const String DEF_SND_SHOT_BLUE = "698,50;659,50";
const String DEF_SND_SHOT_RED = "784,30;1047,30;1319,30";
const String DEF_SND_SHOT_GREEN = "523,30;554,30;523,30";
const String DEF_SND_SHOT_WHITE = "1047,20;1319,20;1568,20;2093,4";
const String DEF_SND_HIT = "2093,30";

String cfg_snd_start, cfg_snd_win, cfg_snd_lose, cfg_snd_mistake;
String cfg_snd_shot_b, cfg_snd_shot_r, cfg_snd_shot_g, cfg_snd_shot_w, cfg_snd_hit;

// Color Configuration
String hex_c1 = "#0000FF";  // Type 1 (Blue)
String hex_c2 = "#FF0000";  // Type 2 (Red)
String hex_c3 = "#00FF00";  // Type 3 (Green)
String hex_c4 = "#FFFF00";  // Boss Mix 1 (Yellow)
String hex_c5 = "#FF00FF";  // Boss Mix 2 (Magenta)
String hex_c6 = "#00FFFF";  // Boss Mix 3 (Cyan)
String hex_cw = "#FFFFFF";  // White Shot
String hex_cb = "#222222";  // Boss Generic / Charging

CRGB col_c1, col_c2, col_c3, col_c4, col_c5, col_c6, col_cw, col_cb;

Melody melStart, melWin, melLose, melMistake, melShotBlue, melShotRed, melShotGreen, melShotWhite, melHit;

// Config
int config_num_leds = 100;
int config_brightness_pct = 50;
int config_start_level = 1;
bool config_sacrifice_led = true;
int config_homebase_size = 3;
int config_shot_speed_pct = 100;
int ledStartOffset = 1;

// AUDIO CONFIG
bool config_sound_on = true;
int config_volume_pct = 50;

String currentProfilePrefix = "def_";
String config_ssid = "";
String config_pass = "";
bool config_static_ip = false;
String config_ip = "";
String config_gateway = "";
String config_subnet = "";
String config_dns = "";
bool wifiMode = false;

// Game State
LevelConfig levels[11];
BossConfig boss1Cfg;
BossConfig boss2Cfg;
BossConfig boss3Cfg;
GameState currentState = STATE_MENU;

unsigned long lastLoopTime = 0;
unsigned long stateTimer = 0;
unsigned long lastShotMove = 0;
unsigned long lastEnemyMove = 0;
unsigned long lastFireTime = 0;
unsigned long bossActionTimer = 0;
bool buttonsReleased = true;

unsigned long btnWhitePressTime = 0;
bool btnWhiteHeld = false;
unsigned long comboTimer = 0;
bool isWaitingForCombo = false;

std::vector<Enemy> enemies;
std::vector<Shot> shots;
std::vector<BossSegment> bossSegments;
std::vector<BossProjectile> bossProjectiles;
float enemyFrontIndex = -1.0;
int currentLevel = 1;
int currentBossType = 0;

Boss2State boss2State = B2_MOVE;
int boss2Section = 0;
int boss2ShotsFired = 0;
int boss2LockedColor = 1;
int markerPos[3];

Boss3State boss3State = B3_MOVE;
int boss3PhaseIndex = 0;
int boss3BurstCounter = 0;
int boss3Markers[2];

int currentScore = 0;
int highScore = 0;
int lastGames[3] = { 0, 0, 0 };
unsigned long levelStartTime = 0;
int levelMaxPossibleScore = 0;
int levelAchievedScore = 0;



