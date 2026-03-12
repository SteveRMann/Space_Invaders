// ==========================================================================
// PROJECT: Space Invders - silent version
// HARDWARE: ESP32D (ESP32 Dev Modile)
// CORE VERSION: 2.0.17 (Required??)
// This version strips the sound and 12s code.
// V2 adds a "continue" button to go to the next level.
// V3 adds "hint" LEDS, renamed White Button to Reset Button
// V4 Adds UDP commands to play sounds
// ==========================================================================


#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <vector>
#include <Update.h>  // Fuer Web-OTA


// UDP
#include <WiFiUdp.h>
WiFiUDP udp;  // Create a UDP object
//const char *SOUND_SERVER_IP = "192.168.1.150";
const char *SOUND_SERVER_IP = "sandbox.local";
const int SOUND_SERVER_PORT = 5005;


// --- LED CONFIGURATION ---
///#define FASTLED_ESP32_S3_PIN 7
///#define FASTLED_RMT_MAX_CHANNELS 1
#include <FastLED.h>




// ============================================================================
// 1. DEFINITIONS & DATA TYPES
// ============================================================================
#define PIN_LED_DATA 15
#define MAX_LEDS 480
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

#define PIN_BTN_BLUE 25
#define PIN_BTN_RED 33
#define PIN_BTN_GREEN 26
#define PIN_BTN_WHITE 32     // Reset, start over
#define PIN_BTN_CONTINUE 27  // Go to the next level

// -----------------------------------------------------
// Colour‑hint GPIOs (external LEDs for the player)
// -----------------------------------------------------
#define PIN_HINT_RED 14
#define PIN_HINT_BLUE 12
#define PIN_HINT_GREEN 13
#define PIN_GPIO19_LED 19              // status LED- follows the sacrificial‑LED blink
static unsigned long lastBlink19 = 0;  // timestamp of the last toggle
static bool gpio19State = false;       // current level of GPIO19 (HIGH/LOW)


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

void saveHighscores();



// ============================================================================
// 2. GLOBAL VARIABLES
// ============================================================================
CRGB leds[MAX_LEDS];
Preferences preferences;
WebServer *server;
//WebServer server(80);
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
int config_num_leds = 475;
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
unsigned long btnContinuePressTime = 0;  // when the continue button was pressed
bool btnContinueHeld = false;            // debounce flag for the continue button
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

// ----- NEW – colour‑hint state ------------------------------------------------
int lastLeadColour = -1;       // colour of the front entity on the previous frame
bool hintPending = false;      // true = we should light the appropriate hint LED
bool hitJustOccurred = false;  // set to true only when a correct shot destroyed the front enemy



// ============================================================================
// 3. HELPER FUNCTIONS
// ============================================================================

/* -------------------- hexToCRGB --------------------------
   Converts a HTML style “#RRGGBB” colour string into a FastLED
   CRGB value (red, green, blue components).
--------------------------------------------------------------- */
CRGB hexToCRGB(String hex) {
  //Converts a HTML style “#RRGGBB” colour string into a FastLED CRGB value (red, green, blue components).
  long number = strtol(&hex[1], NULL, 16);
  return CRGB((number >> 16) & 0xFF, (number >> 8) & 0xFF, number & 0xFF);
}

bool levelJustFinished = false;  // <<< NEW – tells us we just won a level


/* -------------------- Melody ------------------------------------
   Parses a semi colon separated list of “freq,duration” pairs 
   into a std::vector<ToneCmd> (the internal melody representation).
   ----------------------------------------------------------------*/
Melody parseSoundString(String data) {
  Melody m;
  if (data.length() == 0) return m;
  int start = 0;
  int end = data.indexOf(';');
  while (end != -1) {
    String pair = data.substring(start, end);
    int comma = pair.indexOf(',');
    if (comma != -1) {
      ToneCmd t;
      t.freq = pair.substring(0, comma).toInt();
      t.duration = pair.substring(comma + 1).toInt();
      m.push_back(t);
    }
    start = end + 1;
    end = data.indexOf(';', start);
  }
  String pair = data.substring(start);
  int comma = pair.indexOf(',');
  if (comma != -1) {
    ToneCmd t;
    t.freq = pair.substring(0, comma).toInt();
    t.duration = pair.substring(comma + 1).toInt();
    m.push_back(t);
  }
  return m;
}

/* -------------------- melody_FromStr --------------------
   Convenience wrapper that replaces m with the result of
   parseSoundString(s).
   ----------------------------------------------------------- */
void melodyFromStr(Melody &m, String s) {
  m = parseSoundString(s);
}



/* ----------------  abortAndResetGame()-------------------
   1️⃣  Erase all containers that belong to the current run
   Full wipe for a *new* player. Completely wipes all runtime
   containers (enemies, shots, bosses), resets all per run 
   statistics and state flags, and starts the intro for the
   configured start level.
   -------------------------------------------------------- */
void abortAndResetGame() {
  enemies.clear();
  shots.clear();
  bossSegments.clear();
  bossProjectiles.clear();

  // -------------------------------------------------
  // 2️⃣  Reset run‑time statistics and flags
  // -------------------------------------------------
  stat_lastGameShots = 0;
  currentScore = 0;
  levelStartTime = 0;
  levelMaxPossibleScore = 0;
  levelAchievedScore = 0;

  // UI / game‑play flags
  levelJustFinished = false;  // **critical – clears the “continue” flag**
  isWaitingForCombo = false;
  buttonsReleased = true;

  // Timers / state variables (so the next frame starts clean)
  lastLoopTime = 0;
  stateTimer = 0;
  lastShotMove = 0;
  lastEnemyMove = 0;
  lastFireTime = 0;
  bossActionTimer = 0;
  comboTimer = 0;

  // -------------------------------------------------
  // 3️⃣  Reset the *campaign* variables
  // -------------------------------------------------
  // Use the start‑level the user set in the web UI.
  // If you *always* want level 1, simply replace the line below with
  //   currentLevel = 1;
  currentLevel = config_start_level;  // <-- honour UI setting
  currentBossType = 0;                // no boss at the very beginning
  enemyFrontIndex = (float)config_num_leds - 1.0;

  // Boss‑specific state machines – bring them back to their initial state
  boss2Section = 0;
  boss2State = B2_MOVE;
  boss2ShotsFired = 0;
  boss2LockedColor = 1;
  boss3State = B3_MOVE;
  boss3PhaseIndex = 0;
  boss3BurstCounter = 0;

  // -------------------------------------------------
  // 4️⃣  UI / state‑machine – start the *intro* for the chosen level
  // -------------------------------------------------
  currentState = STATE_INTRO;
  startLevelIntro(currentLevel);  // now the intro uses the correct level

  // -------------------------------------------------
  // 5️⃣  Persist high‑score / last‑games (keep total shots/kills)
  // -------------------------------------------------
  saveHighscores();
}


/* ---------------- continueToNextLevel()-----------------------
   Mmove to the next, harder level (keep score)
   Increments the level index, clears level specific containers, 
   re initialises the appropriate boss or enemy wave, starts the
   level intro animation and clears the just finished flag.
   ------------------------------------------------------------ */
void continueToNextLevel() {
  // 1️⃣  Advance the level index
  currentLevel++;

  // 2️⃣  Reset the per‑level containers (enemies, boss pieces, shots…)
  enemies.clear();
  shots.clear();
  bossSegments.clear();
  bossProjectiles.clear();

  // 3️⃣  Reset per‑level timers / flags
  lastShotMove = lastEnemyMove = lastFireTime = bossActionTimer = 0;
  comboTimer = 0;
  isWaitingForCombo = false;
  buttonsReleased = true;
  enemyFrontIndex = (float)config_num_leds - 1.0;
  currentBossType = levels[currentLevel].bossType;

  // 4️⃣  Initialise boss‑specific state machines
  if (currentBossType == 2) {  // Masterblaster (Boss 1)
    boss2Section = 0;
    boss2State = B2_MOVE;
    markerPos[0] = (int)(config_num_leds * (boss2Cfg.m1 / 100.0));
    markerPos[1] = (int)(config_num_leds * (boss2Cfg.m2 / 100.0));
    markerPos[2] = (int)(config_num_leds * (boss2Cfg.m3 / 100.0));
  } else if (currentBossType == 3) {  // RGB Overlord (Boss 3)
    boss3State = B3_MOVE;
    boss3PhaseIndex = 0;
    boss3Markers[0] = (int)(config_num_leds * 0.66);
    boss3Markers[1] = (int)(config_num_leds * 0.50);
    boss3BurstCounter = 0;
  }

  // 5️⃣  Build the level‑specific entities (normal wave or boss)
  if (currentBossType == 0) {  // normal enemies
    int count = levels[currentLevel].length;
    if (count <= 0) count = 10;
    for (int i = 0; i < count; ++i) enemies.push_back({ (int)random(1, 4), 0.0 });
    currentState = STATE_PLAYING;
  } else {                       // any boss
    if (currentBossType == 1) {  // The Tank (boss 2)
      for (int i = 0; i < 9; ++i) bossSegments.push_back({ 0, boss2Cfg.hpPerLed, boss2Cfg.hpPerLed, false, i });
    } else if (currentBossType == 2) {  // Masterblaster (boss 1)
      for (int i = 0; i < 3; ++i) bossSegments.push_back({ 3, boss1Cfg.hpPerLed, boss1Cfg.hpPerLed, true, 0 });
      for (int i = 0; i < 3; ++i) bossSegments.push_back({ 1, boss1Cfg.hpPerLed, boss1Cfg.hpPerLed, true, 0 });
      for (int i = 0; i < 3; ++i) bossSegments.push_back({ 3, boss1Cfg.hpPerLed, boss1Cfg.hpPerLed, true, 0 });
    } else if (currentBossType == 3) {  // RGB Overlord (boss 3)
      for (int i = 0; i < 15; ++i) {
        int mixColor = random(4, 7);
        bossSegments.push_back({ mixColor, boss3Cfg.hpPerLed, boss3Cfg.hpPerLed, true, i });
      }
    }
    currentState = STATE_BOSS_PLAYING;
  }

  // 6️⃣  Kick off the level‑intro animation (so the player sees the new bar)
  startLevelIntro(currentLevel);

  // 7️⃣  We are no longer “just‑finished”; clear the flag.
  levelJustFinished = false;
}

/* ---------------- drawMenu()-----------------------------------
   Optional tiny menu visual (only used if you enable STATE_MENU)
   Simple visual “menu” animation that scrolls blue dots across the
   strip while keeping the home base LEDs white.
   ---------------------------------------------------------------- */
void drawMenu() {
  FastLED.clear();
  for (int i = 0; i < config_num_leds; ++i) {
    if ((i + (millis() / 120)) % 12 < 6) leds[i + ledStartOffset] = CRGB::Blue;
  }
  // keep the home‑base bright so the player still knows where to stand
  for (int i = 0; i < config_homebase_size; ++i) leds[i + ledStartOffset] = CRGB::White;
  FastLED.show();
}


/* ---------------- clearHintLEDs() --------------------
   Turn the hint LEDs off when the game ends
   ------------------------------------------------------- */
void clearHintLEDs() {
  digitalWrite(PIN_HINT_RED, LOW);
  digitalWrite(PIN_HINT_BLUE, LOW);
  digitalWrite(PIN_HINT_GREEN, LOW);
}


// =========================================================================
// 4. AUDIO ENGINE
// =========================================================================
void playToneUDP(int freq, int duration) {
  if (WiFi.status() != WL_CONNECTED) return;  // prevent early crash
  char msg[32];
  snprintf(msg, sizeof(msg), "%d,%d", freq, duration);

  udp.beginPacket(SOUND_SERVER_IP, SOUND_SERVER_PORT);
  udp.write((uint8_t *)msg, strlen(msg));
  udp.endPacket();
}



void playSound(SoundEvent evt) {
  Serial.print("playSound= ");
  Serial.print(evt);
  switch (evt) {
    case 1:  //START
      Serial.println(" Start");
      playToneUDP(523, 80);
      playToneUDP(659, 80);
      playToneUDP(784, 80);
      playToneUDP(1047, 300);
      break;

    case 2:  //WIN
      Serial.println(" Win");
      playToneUDP(523, 80);
      playToneUDP(659, 80);
      playToneUDP(784, 80);
      playToneUDP(1047, 300);
      playToneUDP(0, 150);
      playToneUDP(1047, 60);
      playToneUDP(1319, 60);
      break;

    case 3:  //LOSE
      Serial.println(" Lose");
      playToneUDP(370, 100);
      playToneUDP(349, 100);
      playToneUDP(330, 100);
      playToneUDP(311, 400);
      break;

    case 4:  //MISTAKE
      Serial.println(" Mistake");
      playToneUDP(60, 250);
      break;

    case 5:  //HIT
      Serial.println(" Hit");
      playToneUDP(2093, 60);
      break;

    default:
      Serial.println(" Unknown");
      playToneUDP(100, 750);
  }
}

void playShotSound(int color) {
  Serial.print("playShotSound= ");
  Serial.print(color);
  playToneUDP(800, 250);

  switch (color) {
    case 1:  //Blue button
      Serial.println(" Blue btn");
      playToneUDP(698, 50);
      playToneUDP(659, 50);
      break;
    case 2:  //Red button
      Serial.println(" Red btn");
      playToneUDP(784, 30);
      playToneUDP(1047, 30);
      playToneUDP(1319, 30);
      break;
    case 3:  //Green button
      Serial.println(" Green btn");
      playToneUDP(523, 30);
      playToneUDP(554, 30);
      playToneUDP(523, 30);
      break;
    default:
      Serial.println(" Unknown btn");
      playToneUDP(100, 750);
  }
}



// ==========================================================================
// 5. GRAPHICS ENGINE
// ==========================================================================

/* ------------------ getColor --------------------
   Returns the pre loaded CRGB for a logical colour
   (1 = blue, 2 = red, …, 7 = white, default = black).
   -------------------------------------------------- */
CRGB getColor(int colorCode) {
  switch (colorCode) {
    case 1: return col_c1;
    case 2: return col_c2;
    case 3: return col_c3;
    case 4: return col_c4;
    case 5: return col_c5;
    case 6: return col_c6;
    case 7: return col_cw;
    default: return CRGB::Black;
  }
}


/* ------------------ drawCrispPixel ----------------------
   Rounds the floating LED position to the nearest pixel index
   and writes the given colour into the LED buffer (if the index
   is inside the strip).
   ---------------------------------------------------------- */
void drawCrispPixel(float pos, CRGB color) {
  int idx = round(pos);
  if (idx < 0 || idx >= config_num_leds) return;
  leds[idx + ledStartOffset] = color;
}


/* ------------------- flashPixel --------------------------
   Temporarily lights the LED at pos white (used for hit flashes).
   ---------------------------------------------------------- */
void flashPixel(int pos) {
  if (pos >= 0 && pos < config_num_leds) leds[pos + ledStartOffset] = CRGB::White;
}



// ==========================================================================
// 6. LOGIC & CONFIGURATION (MUST BE BEFORE SETUP)
// ==========================================================================


/* -------------------- saveHighscores ----------------------------------
   Persists the high score, the three most recent game scores, and 
   total‐shots/kills counters into the ESP32 NVS (Preferences).
   ----------------------------------------------------------------------- */
void saveHighscores() {
  preferences.begin("game", false);
  preferences.putInt((currentProfilePrefix + "hs").c_str(), highScore);
  preferences.putInt((currentProfilePrefix + "l1").c_str(), lastGames[0]);
  preferences.putInt((currentProfilePrefix + "l2").c_str(), lastGames[1]);
  preferences.putInt((currentProfilePrefix + "l3").c_str(), lastGames[2]);

  // Save Stats
  preferences.putULong("st_shots", stat_totalShots);
  preferences.putULong("st_kills", stat_totalKills);
  preferences.end();
}


/* -------------------- loadHighscores ---------------------------------- */
void loadHighscores() {
  preferences.begin("game", true);
  highScore = preferences.getInt((currentProfilePrefix + "hs").c_str(), 0);
  lastGames[0] = preferences.getInt((currentProfilePrefix + "l1").c_str(), 0);
  lastGames[1] = preferences.getInt((currentProfilePrefix + "l2").c_str(), 0);
  lastGames[2] = preferences.getInt((currentProfilePrefix + "l3").c_str(), 0);

  stat_totalShots = preferences.getULong("st_shots", 0);
  stat_totalKills = preferences.getULong("st_kills", 0);
  preferences.end();
}


void registerGameEnd(int finalScore) {
  lastGames[2] = lastGames[1];
  lastGames[1] = lastGames[0];
  lastGames[0] = finalScore;
  if (finalScore > highScore) highScore = finalScore;
  saveHighscores();
}

void triggerBaseDestruction() {
  playSound(EVT_LOSE);
  registerGameEnd(currentScore);
  currentState = STATE_BASE_DESTROYED;
  stateTimer = millis();
  clearHintLEDs();
}

void calculateLevelScore() {
  unsigned long duration = millis() - levelStartTime;
  int entityCount = 0;
  if (levels[currentLevel].bossType == 0) entityCount = levels[currentLevel].length;
  else {
    if (levels[currentLevel].bossType == 1) entityCount = 9 * boss1Cfg.hpPerLed;
    else if (levels[currentLevel].bossType == 2) entityCount = 9 * boss2Cfg.hpPerLed;
    else if (levels[currentLevel].bossType == 3) entityCount = 15 * boss3Cfg.hpPerLed;
  }
  int levelMultiplier = currentLevel;
  int basePoints = entityCount * 100 * levelMultiplier;
  unsigned long targetTime = 0;
  if (levels[currentLevel].bossType == 2) targetTime = 38000;
  else {
    unsigned long travelTime = config_num_leds * 15;
    unsigned long processingTime = entityCount * 300;
    targetTime = 3000 + travelTime + processingTime;
  }
  int timeBonus = 0;
  if (levels[currentLevel].bossType == 3) timeBonus = basePoints;
  else {
    if (duration <= targetTime) timeBonus = basePoints;
    else {
      float ratio = (float)targetTime / (float)duration;
      timeBonus = (int)(basePoints * ratio);
    }
  }
  levelAchievedScore = basePoints + timeBonus;
  levelMaxPossibleScore = basePoints * 2;
  currentScore += levelAchievedScore;
}

void checkWinCondition() {
  bool won = false;
  if (currentState == STATE_PLAYING && enemies.empty()) won = true;
  if (currentState == STATE_BOSS_PLAYING && bossSegments.empty()) won = true;
  if (won) {
    calculateLevelScore();
    if (currentLevel >= 10) {
      playSound(EVT_WIN);
      registerGameEnd(currentScore);
      currentState = STATE_GAME_FINISHED;
      clearHintLEDs();
    } else {
      // ----- WIN THIS LEVEL ------------------
      playSound(EVT_WIN);
      currentState = STATE_LEVEL_COMPLETED;
      stateTimer = millis();
      levelJustFinished = true;  // <<< NEW – we are now waiting
    }
  }
}

void startLevelIntro(int level) {
  playSound(EVT_START);
  if (level == config_start_level) {
    currentScore = 0;
    stat_lastGameShots = 0;  // Reset session shots
  }
  currentLevel = level;
  currentState = STATE_INTRO;
  stateTimer = millis();
  FastLED.clear();
  for (int i = 0; i < config_num_leds; i++) leds[i + ledStartOffset] = CRGB(10, 10, 10);
  CRGB barColor = levels[level].bossType > 0 ? col_c2 : col_c3;
  int center = config_num_leds / 2;
  int totalWidth = (level * 6) + ((level - 1) * 4);
  int startPos = center - (totalWidth / 2);
  if (startPos < 0) startPos = 0;
  int cursor = startPos;
  for (int i = 0; i < level; i++) {
    for (int k = 0; k < 6; k++) {
      if (cursor < config_num_leds) leds[cursor + ledStartOffset] = barColor;
      cursor++;
    }
    cursor += 4;
  }
  if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
  FastLED.show();
}


/* -------------------- drawLevelIntro ----------------------
   Helper that draws the static bar used by the intro animation 
   (used repeatedly while the intro timer runs).
   ---------------------------------------------------------- */
void drawLevelIntro(int level) {
  FastLED.clear();
  for (int i = 0; i < config_num_leds; i++) leds[i + ledStartOffset] = CRGB(5, 5, 5);
  CRGB barColor = levels[level].bossType > 0 ? col_c2 : col_c3;
  int center = config_num_leds / 2;
  int totalWidth = (level * 6) + ((level - 1) * 4);
  int startPos = center - (totalWidth / 2);
  if (startPos < 0) startPos = 0;
  int cursor = startPos;
  for (int i = 0; i < level; i++) {
    for (int k = 0; k < 6; k++) {
      if (cursor < config_num_leds) leds[cursor + ledStartOffset] = barColor;
      cursor++;
    }
    cursor += 4;
  }
  if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
  FastLED.show();
}



/* ----------------- updateLevelIntro ------------------------------
   Drives the timed intro animation – flashing the bar, waiting 2 s, then 
   initialising the enemy or boss data and switching to STATE_PLAYING/STATE_BOSS_PLAYING.
   ----------------------------------------------------------------- */
void updateLevelIntro() {
  unsigned long elapsed = millis() - stateTimer;
  if (elapsed > 2000 && elapsed < 4000) {
    if ((elapsed / 250) % 2 == 0) drawLevelIntro(currentLevel);
    else {
      FastLED.clear();
      if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
      FastLED.show();
    }
  } else if (elapsed <= 2000) {
    drawLevelIntro(currentLevel);
  }

  if (elapsed >= 4000) {
    uint8_t bright = map(config_brightness_pct, 10, 100, 25, 255);
    FastLED.setBrightness(bright);
    levelStartTime = millis();
    if (levels[currentLevel].bossType > 0) {
      currentBossType = levels[currentLevel].bossType;
      bossSegments.clear();
      enemies.clear();
      shots.clear();
      bossProjectiles.clear();
      enemyFrontIndex = (float)config_num_leds - 1.0;
      if (currentBossType == 1) {
        for (int i = 0; i < 3; i++) bossSegments.push_back({ 3, boss1Cfg.hpPerLed, boss1Cfg.hpPerLed, true, 0 });
        for (int i = 0; i < 3; i++) bossSegments.push_back({ 1, boss1Cfg.hpPerLed, boss1Cfg.hpPerLed, true, 0 });
        for (int i = 0; i < 3; i++) bossSegments.push_back({ 3, boss1Cfg.hpPerLed, boss1Cfg.hpPerLed, true, 0 });
        bossActionTimer = millis();
      } else if (currentBossType == 2) {
        for (int i = 0; i < 9; i++) bossSegments.push_back({ 0, boss2Cfg.hpPerLed, boss2Cfg.hpPerLed, false, i });
        boss2Section = 0;
        boss2State = B2_MOVE;
        markerPos[0] = (int)(config_num_leds * (boss2Cfg.m1 / 100.0));
        markerPos[1] = (int)(config_num_leds * (boss2Cfg.m2 / 100.0));
        markerPos[2] = (int)(config_num_leds * (boss2Cfg.m3 / 100.0));
      } else if (currentBossType == 3) {
        for (int i = 0; i < 15; i++) {
          int mixColor = random(4, 7);
          bossSegments.push_back({ mixColor, boss3Cfg.hpPerLed, boss3Cfg.hpPerLed, true, i });
        }
        boss3State = B3_MOVE;
        boss3PhaseIndex = 0;
        boss3Markers[0] = (int)(config_num_leds * 0.66);
        boss3Markers[1] = (int)(config_num_leds * 0.50);
        bossActionTimer = millis();
      }
      currentState = STATE_BOSS_PLAYING;
    } else {
      enemies.clear();
      shots.clear();
      bossProjectiles.clear();
      int count = levels[currentLevel].length;
      if (count <= 0) count = 10;

      for (int i = 0; i < count; i++) {
        enemies.push_back({ (int)random(1, 4), 0.0 });
      }
      enemyFrontIndex = (float)config_num_leds - 1.0;
      currentState = STATE_PLAYING;
    }
  }
}


/* ----------------- updateLevelCompletedAnim ----------------------------
   Shows the “level cleared” animation: a solid colour for 1 s, then a progress
   bar that visualises the ratio of achieved score to maximum possible score.
   ----------------------------------------------------------------------- */
void updateLevelCompletedAnim() {
  unsigned long elapsed = millis() - stateTimer;
  if (elapsed < 1000) {
    fill_solid(leds, config_num_leds + ledStartOffset, col_c3);
    if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
  } else if (elapsed < 5000) {
    FastLED.clear();
    if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
    float pct = (float)levelAchievedScore / (float)levelMaxPossibleScore;
    if (pct > 1.0) pct = 1.0;
    int fillLeds = (int)(config_num_leds * pct);
    for (int i = 0; i < fillLeds; i++) leds[i + ledStartOffset] = CRGB(80, 60, 0);
    for (int i = fillLeds; i < config_num_leds; i++) leds[i + ledStartOffset] = CRGB(20, 0, 0);
  } else {
    ///startLevelIntro(currentLevel + 1);
  }
  FastLED.show();
}


/* ----------------- updateBaseDestroyedAnim --------------------
   Blinks the home base LEDs between red and white for 2 s, then
   sets the state to STATE_GAMEOVER.
   -------------------------------------------------------------- */
void updateBaseDestroyedAnim() {
  unsigned long elapsed = millis() - stateTimer;
  if (elapsed < 2000) {
    CRGB c = (elapsed / 100) % 2 == 0 ? col_c2 : CRGB::White;
    for (int i = 0; i < config_homebase_size; i++) {
      if (i + ledStartOffset < config_num_leds) leds[i + ledStartOffset] = c;
    }
    for (int i = config_homebase_size; i < config_num_leds; i++) {
      leds[i + ledStartOffset].nscale8(240);
    }
    if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
    FastLED.show();
  } else {
    currentState = STATE_GAMEOVER;
    clearHintLEDs();
  }
}


/* ------------------ moveBossProjectiles --------------------------------
   Advances all active boss projectiles toward the home base at the given
   speed; if any reach the base the game ends via triggerBaseDestruction().
   ---------------------------------------------------------------------- */
void moveBossProjectiles(float speed) {
  static unsigned long lastMove = 0;
  float step = (float)speed / 60.0;
  if (step < 0.1) step = 0.1;

  for (int i = bossProjectiles.size() - 1; i >= 0; i--) {
    bossProjectiles[i].pos -= step;
    if (bossProjectiles[i].pos < config_homebase_size) {
      triggerBaseDestruction();
    }
  }
}



// ==========================================================================
// 7. CONFIG & WEB HANDLERS (MUST BE BEFORE SETUP)
// ==========================================================================

/* ------------------------ loadColors ------------------------------------
   Reads saved hex colour strings from NVS, converts them with hexToCRGB,
   and stores the resulting CRGB globals (col_c1, col_c2, …).
   ------------------------------------------------------------------------ */
void loadColors() {
  preferences.begin("colors", true);
  hex_c1 = preferences.getString("c1", "#0000FF");
  hex_c2 = preferences.getString("c2", "#FF0000");
  hex_c3 = preferences.getString("c3", "#00FF00");
  hex_c4 = preferences.getString("c4", "#FFFF00");
  hex_c5 = preferences.getString("c5", "#FF00FF");
  hex_c6 = preferences.getString("c6", "#00FFFF");
  hex_cw = preferences.getString("cw", "#FFFFFF");
  hex_cb = preferences.getString("cb", "#222222");
  preferences.end();

  col_c1 = hexToCRGB(hex_c1);
  col_c2 = hexToCRGB(hex_c2);
  col_c3 = hexToCRGB(hex_c3);
  col_c4 = hexToCRGB(hex_c4);
  col_c5 = hexToCRGB(hex_c5);
  col_c6 = hexToCRGB(hex_c6);
  col_cw = hexToCRGB(hex_cw);
  col_cb = hexToCRGB(hex_cb);
}



/* ----------------------- handleSaveColors --------------------------------
   HTTP POST handler that receives new hex colour values, writes them to NVS,
   reloads the colour globals, and redirects back to the colour page.
   ------------------------------------------------------------------------- */
void handleSaveColors() {
  if (server->hasArg("c1")) hex_c1 = server->arg("c1");
  if (server->hasArg("c2")) hex_c2 = server->arg("c2");
  if (server->hasArg("c3")) hex_c3 = server->arg("c3");
  if (server->hasArg("c4")) hex_c4 = server->arg("c4");
  if (server->hasArg("c5")) hex_c5 = server->arg("c5");
  if (server->hasArg("c6")) hex_c6 = server->arg("c6");
  if (server->hasArg("cw")) hex_cw = server->arg("cw");
  if (server->hasArg("cb")) hex_cb = server->arg("cb");

  preferences.begin("colors", false);
  preferences.putString("c1", hex_c1);
  preferences.putString("c2", hex_c2);
  preferences.putString("c3", hex_c3);
  preferences.putString("c4", hex_c4);
  preferences.putString("c5", hex_c5);
  preferences.putString("c6", hex_c6);
  preferences.putString("cw", hex_cw);
  preferences.putString("cb", hex_cb);
  preferences.end();

  loadColors();
  server->sendHeader("Location", "/colors");
  server->send(303);
}


/* ---------------------- loadSounds -----------------------------
   Reads saved sound string definitions from NVS (or defaults)and
   parses each into a Melody using melodyFromStr.
------------------------------------------------------------------ */
void loadSounds() {
  preferences.begin("snds", true);
  cfg_snd_start = preferences.getString("s_start", DEF_SND_START);
  cfg_snd_win = preferences.getString("s_win", DEF_SND_WIN);
  cfg_snd_lose = preferences.getString("s_lose", DEF_SND_LOSE);
  cfg_snd_mistake = preferences.getString("s_mistake", DEF_SND_MISTAKE);
  cfg_snd_hit = preferences.getString("s_hit", DEF_SND_HIT);
  cfg_snd_shot_b = preferences.getString("s_shot_b", DEF_SND_SHOT_BLUE);
  cfg_snd_shot_r = preferences.getString("s_shot_r", DEF_SND_SHOT_RED);
  cfg_snd_shot_g = preferences.getString("s_shot_g", DEF_SND_SHOT_GREEN);
  cfg_snd_shot_w = preferences.getString("s_shot_w", DEF_SND_SHOT_WHITE);
  preferences.end();

  melodyFromStr(melStart, cfg_snd_start);
  melodyFromStr(melWin, cfg_snd_win);
  melodyFromStr(melLose, cfg_snd_lose);
  melodyFromStr(melMistake, cfg_snd_mistake);
  melodyFromStr(melHit, cfg_snd_hit);
  melodyFromStr(melShotBlue, cfg_snd_shot_b);
  melodyFromStr(melShotRed, cfg_snd_shot_r);
  melodyFromStr(melShotGreen, cfg_snd_shot_g);
  melodyFromStr(melShotWhite, cfg_snd_shot_w);
}



/* ---------------------- applyProfileDefaults -------------------------
   Fills the level  and boss configuration arrays with the default values 
   for the three built in profiles (def_, kid_, pro_).
   --------------------------------------------------------------------- */
void applyProfileDefaults(String prefix) {
  if (prefix == "def_") {
    // STANDARD PROFILE
    levels[1] = { 5, 15, 0 };
    levels[2] = { 6, 20, 0 };
    levels[3] = { 7, 25, 2 };  // Masterblaster
    levels[4] = { 8, 30, 0 };
    levels[5] = { 9, 35, 0 };
    levels[6] = { 10, 40, 1 };  // The Tank
    levels[7] = { 20, 20, 0 };
    levels[8] = { 20, 25, 0 };
    levels[9] = { 10, 60, 0 };
    levels[10] = { 14, 60, 3 };
    boss1Cfg = { 4, 60, 4, 30, 0, 0, 0, 0 };
    boss2Cfg = { 10, 60, 5, 40, 0, 85, 55, 30 };
    boss3Cfg = { 7, 50, 3, 60, 5, 0, 0, 0 };
  } else if (prefix == "kid_") {
    // KIDS PROFILE (EASY)
    levels[1] = { 5, 15, 0 };
    levels[2] = { 5, 20, 0 };
    levels[3] = { 6, 25, 2 };
    levels[4] = { 6, 20, 0 };
    levels[5] = { 7, 25, 0 };
    levels[6] = { 10, 40, 1 };
    levels[7] = { 8, 30, 0 };
    levels[8] = { 8, 35, 0 };
    levels[9] = { 10, 20, 0 };
    levels[10] = { 14, 60, 3 };
    boss1Cfg = { 4, 60, 2, 40, 0, 0, 0, 0 };
    boss2Cfg = { 7, 40, 3, 40, 0, 85, 55, 30 };
    boss3Cfg = { 4, 40, 1, 80, 1, 0, 0, 0 };
  } else {
    // PRO PROFILE
    for (int i = 1; i <= 10; i++) { levels[i] = { 5 + i, 15 + (i * 5), 0 }; }
    levels[3].bossType = 2;
    levels[6].bossType = 1;
    levels[10].bossType = 3;
    boss1Cfg = { 6, 80, 5, 25, 0, 0, 0, 0 };
    boss2Cfg = { 15, 80, 6, 30, 0, 90, 60, 30 };
    boss3Cfg = { 15, 60, 6, 40, 8, 0, 0, 0 };
  }
}


/* --------------- saveCurrentToPreferences ---------------------
   Writes the current LED, brightness, start level, level data and
   boss parameters into NVS under the given profile prefix.
   ------------------------------------------------------------- */
void saveCurrentToPreferences(String prefix) {
  preferences.begin("game", false);
  preferences.putInt((prefix + "leds").c_str(), config_num_leds);
  preferences.putInt((prefix + "bright").c_str(), config_brightness_pct);
  preferences.putInt((prefix + "startlvl").c_str(), config_start_level);

  for (int i = 1; i <= 10; i++) {
    preferences.putInt((prefix + "l" + String(i) + "s").c_str(), levels[i].speed);
    preferences.putInt((prefix + "l" + String(i) + "l").c_str(), levels[i].length);
    preferences.putInt((prefix + "l" + String(i) + "b").c_str(), levels[i].bossType);
  }
  preferences.putBytes((prefix + "b1").c_str(), &boss1Cfg, sizeof(BossConfig));
  preferences.putBytes((prefix + "b2").c_str(), &boss2Cfg, sizeof(BossConfig));
  preferences.putBytes((prefix + "b3").c_str(), &boss3Cfg, sizeof(BossConfig));
  preferences.end();
}


/* ------------------ performFactoryReset ---------------------------
   Erases all user settings, restores the three default profiles,
   re applies them, and writes a fresh configuration version flag.
   ------------------------------------------------------------------ */
void performFactoryReset() {
  preferences.begin("game", true);
  String s = preferences.getString("ssid", "");
  String p = preferences.getString("pass", "");
  String ip = preferences.getString("sip_ip", "");
  String gw = preferences.getString("sip_gw", "");
  String sn = preferences.getString("sip_sn", "");
  String dns = preferences.getString("sip_dns", "");
  bool sip = preferences.getBool("sip_on", false);
  preferences.end();

  preferences.begin("game", false);
  preferences.clear();
  preferences.putString("ssid", s);
  preferences.putString("pass", p);
  preferences.putBool("sip_on", sip);
  preferences.putString("sip_ip", ip);
  preferences.putString("sip_gw", gw);
  preferences.putString("sip_sn", sn);
  preferences.putString("sip_dns", dns);

  preferences.putBool("sac_led", true);
  preferences.putInt("hb_size", 3);
  preferences.putInt("shot_spd", 100);

  preferences.putBool("snd_on", true);
  preferences.putInt("snd_vol", 50);

  // STATS RESET? Typically factory reset wipes stats too.
  preferences.putULong("st_shots", 0);
  preferences.putULong("st_kills", 0);

  preferences.putInt("version", CONFIG_VERSION);
  preferences.putString("act_prof", "def_");
  preferences.end();

  preferences.begin("snds", false);
  preferences.clear();
  preferences.end();
  preferences.begin("colors", false);
  preferences.clear();
  preferences.end();

  applyProfileDefaults("def_");
  saveCurrentToPreferences("def_");
  applyProfileDefaults("kid_");
  saveCurrentToPreferences("kid_");
  applyProfileDefaults("pro_");
  saveCurrentToPreferences("pro_");
  applyProfileDefaults("def_");
}

void setupDefaultConfig() {
  applyProfileDefaults("def_");
}

void loadConfig(String prefix) {
  // Convenience wrapper that simply loads the “standard” profile defaults (def_).
  preferences.begin("game", true);
  config_num_leds = preferences.getInt((prefix + "leds").c_str(), config_num_leds);
  config_brightness_pct = preferences.getInt((prefix + "bright").c_str(), config_brightness_pct);
  config_start_level = preferences.getInt((prefix + "startlvl").c_str(), config_start_level);
  config_ssid = preferences.getString("ssid", "");
  config_pass = preferences.getString("pass", "");
  config_static_ip = preferences.getBool("sip_on", false);
  config_ip = preferences.getString("sip_ip", "");
  config_gateway = preferences.getString("sip_gw", "");
  config_subnet = preferences.getString("sip_sn", "");
  config_dns = preferences.getString("sip_dns", "");

  config_sacrifice_led = preferences.getBool("sac_led", true);
  config_homebase_size = preferences.getInt("hb_size", 3);
  config_shot_speed_pct = preferences.getInt("shot_spd", 100);
  ledStartOffset = config_sacrifice_led ? 1 : 0;

  config_sound_on = preferences.getBool("snd_on", true);
  config_volume_pct = preferences.getInt("snd_vol", 50);

  for (int i = 1; i <= 10; i++) {
    levels[i].speed = preferences.getInt((prefix + "l" + String(i) + "s").c_str(), levels[i].speed);
    levels[i].length = preferences.getInt((prefix + "l" + String(i) + "l").c_str(), levels[i].length);
    levels[i].bossType = preferences.getInt((prefix + "l" + String(i) + "b").c_str(), levels[i].bossType);
  }

  if (preferences.isKey((prefix + "b1").c_str())) preferences.getBytes((prefix + "b1").c_str(), &boss1Cfg, sizeof(BossConfig));
  if (preferences.isKey((prefix + "b2").c_str())) preferences.getBytes((prefix + "b2").c_str(), &boss2Cfg, sizeof(BossConfig));
  if (preferences.isKey((prefix + "b3").c_str())) preferences.getBytes((prefix + "b3").c_str(), &boss3Cfg, sizeof(BossConfig));
  preferences.end();
}


/* --------------------- handleProfileSwitch --------------------------
   HTTP POST handler that changes the active profile (def_, kid_, or pro_),
   saves the selection, reloads defaults and redirects to the main page.
   ---------------------------------------------------------------------*/
void handleProfileSwitch() {
  if (server->hasArg("profile")) {
    String p = server->arg("profile");
    if (p == "kid") currentProfilePrefix = "kid_";
    else if (p == "pro") currentProfilePrefix = "pro_";
    else currentProfilePrefix = "def_";
    preferences.begin("game", false);
    preferences.putString("act_prof", currentProfilePrefix);
    preferences.end();
    applyProfileDefaults(currentProfilePrefix);
    loadConfig(currentProfilePrefix);
    loadHighscores();
    server->sendHeader("Location", "/");
    server->send(303);
  } else server->send(400, "text/plain", "Bad Request");
}


/* ------------------- handleReset ---------------------
   HTTP POST handler that runs performFactoryReset(), 
   notifies the user and restarts the ESP.
   ---------------------------------------------------------- */
void handleReset() {
  performFactoryReset();
  server->send(200, "text/html", "<h2>Reset successful!</h2><p>Values & Scores wiped. ESP restarting.</p>");
  delay(1000);
  ESP.restart();
}


/* ------------------------ handleSave ----------------------------
   HTTP POST handler for the main configuration page; parses all
   form fields, validates LED count, stores the new settings in NVS,
   and restarts the ESP to apply them.
   --------------------------------------------------------------- */
void handleSave() {
  // HTTP POST handler that runs performFactoryReset(), notifies the user and restarts the ESP.
  if (server->hasArg("leds")) config_num_leds = server->arg("leds").toInt();

  // --- SANITIZE LED COUNT FROM UI ---
  if (config_num_leds < 30) config_num_leds = 30;
  if (config_num_leds > MAX_LEDS) config_num_leds = MAX_LEDS;

  if (server->hasArg("bright")) config_brightness_pct = server->arg("bright").toInt();
  if (server->hasArg("startlvl")) config_start_level = server->arg("startlvl").toInt();
  if (server->hasArg("ssid")) config_ssid = server->arg("ssid");
  if (server->hasArg("pass")) config_pass = server->arg("pass");
  config_static_ip = server->hasArg("static_ip");
  config_ip = server->arg("ip");
  config_gateway = server->arg("gw");
  config_subnet = server->arg("sn");
  config_dns = server->arg("dns");
  if (server->hasArg("hb_size")) config_homebase_size = server->arg("hb_size").toInt();
  if (server->hasArg("shot_spd")) config_shot_speed_pct = server->arg("shot_spd").toInt();
  config_sacrifice_led = server->hasArg("sac_led");

  config_sound_on = server->hasArg("snd_on");
  if (server->hasArg("vol")) config_volume_pct = server->arg("vol").toInt();

  preferences.begin("game", false);
  preferences.putInt("version", CONFIG_VERSION);
  preferences.putString("ssid", config_ssid);
  preferences.putString("pass", config_pass);
  preferences.putBool("sip_on", config_static_ip);
  preferences.putString("sip_ip", config_ip);
  preferences.putString("sip_gw", config_gateway);
  preferences.putString("sip_sn", config_subnet);
  preferences.putString("sip_dns", config_dns);
  preferences.putBool("sac_led", config_sacrifice_led);
  preferences.putInt("hb_size", config_homebase_size);
  preferences.putInt("shot_spd", config_shot_speed_pct);

  preferences.putBool("snd_on", config_sound_on);
  preferences.putInt("snd_vol", config_volume_pct);

  String p = currentProfilePrefix;
  preferences.putInt((p + "leds").c_str(), config_num_leds);
  preferences.putInt((p + "bright").c_str(), config_brightness_pct);
  preferences.putInt((p + "startlvl").c_str(), config_start_level);
  for (int i = 1; i <= 10; i++) {
    levels[i].speed = server->arg("lspd" + String(i)).toInt();
    levels[i].length = server->arg("llen" + String(i)).toInt();
    levels[i].bossType = server->arg("lboss" + String(i)).toInt();
    preferences.putInt((p + "l" + String(i) + "s").c_str(), levels[i].speed);
    preferences.putInt((p + "l" + String(i) + "l").c_str(), levels[i].length);
    preferences.putInt((p + "l" + String(i) + "b").c_str(), levels[i].bossType);
  }
  boss1Cfg.moveSpeed = server->arg("b1mv").toInt();
  boss1Cfg.shotSpeed = server->arg("b1ss").toInt();
  boss1Cfg.hpPerLed = server->arg("b1hp").toInt();
  boss1Cfg.shotFreq = server->arg("b1fr").toInt();
  preferences.putBytes((p + "b1").c_str(), &boss1Cfg, sizeof(BossConfig));
  boss2Cfg.moveSpeed = server->arg("b2mv").toInt();
  boss2Cfg.shotSpeed = server->arg("b2ss").toInt();
  boss2Cfg.hpPerLed = server->arg("b2hp").toInt();
  boss2Cfg.shotFreq = server->arg("b2fr").toInt();
  boss2Cfg.m1 = server->arg("b2m1").toInt();
  boss2Cfg.m2 = server->arg("b2m2").toInt();
  boss2Cfg.m3 = server->arg("b2m3").toInt();
  preferences.putBytes((p + "b2").c_str(), &boss2Cfg, sizeof(BossConfig));
  boss3Cfg.moveSpeed = server->arg("b3mv").toInt();
  boss3Cfg.shotSpeed = 0;
  boss3Cfg.hpPerLed = server->arg("b3hp").toInt();
  boss3Cfg.shotFreq = server->arg("b3fr").toInt();
  boss3Cfg.burstCount = server->arg("b3bc").toInt();
  preferences.putBytes((p + "b3").c_str(), &boss3Cfg, sizeof(BossConfig));

  preferences.end();

  // Redirect browser to root, then restart so new settings take effect
  server->sendHeader("Location", "/");
  server->send(303);

  delay(500);     // give the TCP stack a moment to send the redirect
  ESP.restart();  // reboot and apply saved settings
}


/* ------------------------- handleContinue ---------------------------
   HTTP POST handler for the “/next” endpoint; if a level was just won
   (levelJustFinished == true) it calls continueToNextLevel() and
   returns a 200 response, otherwise a 400 error.
   ------------------------------------------------------------------- */
void handleContinue() {
  if (levelJustFinished) {
    continueToNextLevel();
    server->send(200, "text/plain", "Next level started");
  } else {
    server->send(400, "text/plain", "No finished level to continue");
  }
}


String getUpdateHTML() {
  // Returns the full HTML page used for OTA firmware updates (file upload form).
  String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  h += "<title>Firmware Update</title>";
  h += "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:20px;max-width:600px;margin:auto;} h2{color:#0f0;} input{width:100%;margin-bottom:10px;padding:10px;}</style>";
  h += "</head><body><h2>SYSTEM UPDATE</h2>";
  h += "<p>Upload .bin file from Arduino IDE (Sketch -> Export Compiled Binary).</p>";
  h += "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><br><button type='submit' style='padding:15px;background:#00f;color:white;font-weight:bold;width:100%;cursor:pointer;'>UPDATE FIRMWARE</button></form>";
  h += "<br><a href='/' style='color:#0f0;'>Back</a></body></html>";
  return h;
}


String getColorHTML() {
  // Returns the HTML page that lets the user pick custom colours for enemies, bosses, and player shots.
  String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  h += "<title>Color Config</title>";
  h += "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:10px;max-width:800px;margin:auto;}input,button{width:100%;background:#333;color:#fff;border:1px solid #555;padding:8px;border-radius:4px;box-sizing:border-box;margin-bottom:5px;} .sec{margin-top:15px;border-top:1px solid #555;padding-top:15px;background:#222;padding:15px;border-radius:5px;} h3{margin-top:0;color:#0f0;} input[type=color] { height: 50px; cursor: pointer; } a { color: #00ff00; text-decoration: none; }</style>";
  h += "</head><body>";
  h += "<h2>🎨 CUSTOM COLORS</h2>";
  h += "<form action='/savecolors' method='POST'>";

  h += "<div class='sec'><h3>Main Enemies (Types 1-3)</h3>";
  h += "Type 1 (Blue btn): <input type='color' name='c1' value='" + hex_c1 + "'>";
  h += "Type 2 (Red btn): <input type='color' name='c2' value='" + hex_c2 + "'>";
  h += "Type 3 (Green btn): <input type='color' name='c3' value='" + hex_c3 + "'></div>";

  h += "<div class='sec'><h3>Boss / Mix Colors</h3>";
  h += "Mix Color 1: <input type='color' name='c4' value='" + hex_c4 + "'>";
  h += "Mix Color 2: <input type='color' name='c5' value='" + hex_c5 + "'>";
  h += "Mix Color 3: <input type='color' name='c6' value='" + hex_c6 + "'>";
  h += "Boss Generic/Dark: <input type='color' name='cb' value='" + hex_cb + "'></div>";

  h += "<div class='sec'><h3>Player Shots</h3>";
  h += "Combo Shot (White): <input type='color' name='cw' value='" + hex_cw + "'></div>";

  h += "<br><button type='submit' style='background:#009900;font-size:1.2em;'>SAVE COLORS</button>";
  h += "</form><br><a href='/'>&laquo; Back to Main Menu</a>";
  h += "</body></html>";
  return h;
}


/* ------------------------ getSoundHTML -------------------------
   Returns the HTML page that lets the user edit the frequency/duration
   strings for the various game sound events.
---------------------------------------------------------------- */
String getSoundHTML() {
  String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  h += "<title>Sound Config</title>";
  h += "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:10px;max-width:800px;margin:auto;}input,button{width:100%;background:#333;color:#fff;border:1px solid #555;padding:8px;border-radius:4px;box-sizing:border-box;margin-bottom:5px;} .sec{margin-top:15px;border-top:1px solid #555;padding-top:15px;background:#222;padding:15px;border-radius:5px;} h3{margin-top:0;color:#0f0;} .info{font-size:0.9em;color:#aaa;background:#333;padding:10px;border-radius:4px;margin-bottom:15px;} a { color: #00ff00; text-decoration: none; }</style>";
  h += "</head><body>";
  h += "<h2>🎵 CUSTOM SOUND EDITOR</h2>";
  h += "<div class='info'><b>How to use:</b><br>Format: <code>Freq,Duration;Freq,Duration</code><br>Example: <code>523,100;659,100</code> (Plays C5 then E5)<br>Leave empty for silence.<br><br><b>Ask Gemini/AI:</b><br><i>\"Create a sound string for [Effect] using pairs of Frequency(Hz),Duration(ms) separated by semicolons. E.g. a rising laser sound.\"</i></div>";
  h += "<form action='/savesounds' method='POST'>";
  h += "<div class='sec'><h3>Game Events</h3>";
  h += "Start: <input name='s_start' value='" + cfg_snd_start + "'>";
  h += "Win Level: <input name='s_win' value='" + cfg_snd_win + "'>";
  h += "Game Over: <input name='s_lose' value='" + cfg_snd_lose + "'>";
  h += "Mistake/Grow: <input name='s_mistake' value='" + cfg_snd_mistake + "'>";
  h += "Hit Success: <input name='s_hit' value='" + cfg_snd_hit + "'></div>";
  h += "<div class='sec'><h3>Player Shots</h3>";
  h += "Blue Shot: <input name='s_shot_b' value='" + cfg_snd_shot_b + "'>";
  h += "Red Shot: <input name='s_shot_r' value='" + cfg_snd_shot_r + "'>";
  h += "Green Shot: <input name='s_shot_g' value='" + cfg_snd_shot_g + "'>";
  h += "White Shot: <input name='s_shot_w' value='" + cfg_snd_shot_w + "'></div>";
  h += "<br><button type='submit' style='background:#009900;font-size:1.2em;'>SAVE SOUNDS</button>";
  h += "</form><br><a href='/'>&laquo; Back to Main Menu</a>";
  h += "</body></html>";
  return h;
}

/* -------------------------------- getHTML -------------------------------
   Returns the main configuration web page (LED count, brightness, Wi Fi,
   level table, boss parameters, stats, and navigation links).
   --------------------------------------------------------------- */
String getHTML() {
  String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  h += "<title>Ultimate RGB Invaders</title>";
  h += "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:10px;max-width:800px;margin:auto;}input,select,button{width:100%;background:#333;color:#fff;border:1px solid #555;padding:8px;border-radius:4px;box-sizing:border-box;margin-bottom:5px;}table{width:100%;border-collapse:collapse;margin-bottom:10px;} td,th{border:1px solid #444;padding:6px;text-align:center;}.sec{margin-top:25px;border-top:1px solid #555;padding-top:15px;background:#222;padding:15px;border-radius:5px;}#warning-box{margin-top:10px;padding:10px;background:#b30000;color:#fff;border:1px solid #ff0000;display:none;font-weight:bold;border-radius:4px;}.val-highlight{color:#0f0;font-weight:bold;}h2,h3{margin-top:0;} pre{font-family:monospace;color:#0f0;font-size:12px;overflow-x:auto;} .score-box{background:#004d00; border:2px solid #00ff00; padding:15px; text-align:center; margin-bottom:15px; border-radius:8px;} .big-score{font-size:32px; font-weight:bold; color:#fff;} .small-score{font-size:14px; color:#aaa;}";
  h += " .b-card { background: #2a2a2a; padding: 15px; border-radius: 5px; margin-bottom: 15px; border: 1px solid #444; }";
  h += " .b-head { font-size: 1.1em; font-weight: bold; margin-bottom: 10px; color: #fff; border-bottom: 1px solid #555; padding-bottom: 5px; }";
  h += " .form-grid { display: grid; grid-template-columns: 1fr 1fr 1fr 1fr; gap: 10px; }";
  h += " .f-item { display: flex; flex-direction: column; }";
  h += " .lbl { font-size: 11px; color: #aaa; margin-bottom: 4px; }";
  h += " .neon-text { font-family: sans-serif; font-weight: 900; font-size: 3.8em; text-align: center; text-transform: uppercase; letter-spacing: 4px; margin: 20px 0; background: linear-gradient(90deg, #ff0000, #ffff00, #00ff00, #00ffff, #0000ff, #ff00ff, #ff0000); background-size: 400%; -webkit-background-clip: text; background-clip: text; color: rgba(255,255,255,0.1); animation: rgbAnim 4s linear infinite; text-shadow: 0 0 20px rgba(255,255,255,0.2); }";
  h += " @keyframes rgbAnim { 0% { background-position: 0%; } 100% { background-position: 400%; } }";
  h += " .sub-head { text-align: center; font-size: 0.8em; color: #888; margin-bottom: 20px; }";
  h += " .credits { text-align: center; margin-top: 30px; font-size: 0.8em; color: #555; border-top: 1px solid #333; padding-top: 10px; }";
  h += " a { color: #00ff00; text-decoration: none; }";
  h += " .stat-grid { display:flex; gap:10px; text-align:center; } .stat-box { flex:1; background:#222; padding:10px; border-radius:5px; border:1px solid #444; } .stat-val { font-size:1.5em; font-weight:bold; color:#0ff; }";
  h += "</style>";
  h += "<script>function updateCalc() { var count = parseInt(document.getElementById('ledCount').value)||0; var brightPct = parseInt(document.getElementById('brightness').value)||50; document.getElementById('brightVal').innerText = brightPct + '%'; var warn = document.getElementById('warning-box'); if (count > 0) { var brightFactor = brightPct / 100.0; var amps = ((count * 15 * brightFactor) + 120) / 1000; document.getElementById('ampValue').innerText = amps.toFixed(2) + ' A'; if(amps > 5.0) { warn.style.display='block'; warn.innerText='WARNING: > 5A! High Power PSU required!'; } else warn.style.display='none'; } } function toggleIP() { var x = document.getElementById('ipsettings'); if(document.getElementById('chkStatic').checked) x.style.display='block'; else x.style.display='none'; } function confirmReset() { return confirm('Really delete all settings and factory reset?'); } window.onload = function(){ updateCalc(); toggleIP(); };</script>";
  h += "</head><body>";
  h += "<div class='neon-text'>RGB INVADERS</div>";
  h += "<div class='sub-head'>created by Qwer.Tzui / WorksAsDesigned - Version 10.0 (Final)</div>";
  h += "<div class='score-box'>ALL TIME BEST<div class='big-score'>" + String(highScore) + "</div>";
  h += "<div class='small-score'>Last Games: " + String(lastGames[0]) + " | " + String(lastGames[1]) + " | " + String(lastGames[2]) + "</div></div>";

  // STATS DISPLAY
  h += "<div class='sec'><h3>Battle Statistics</h3><div class='stat-grid'>";
  h += "<div class='stat-box'><div class='stat-val'>" + String(stat_totalShots) + "</div><div>Total Shots</div></div>";
  h += "<div class='stat-box'><div class='stat-val'>" + String(stat_totalKills) + "</div><div>Alien Kills</div></div>";
  h += "<div class='stat-box'><div class='stat-val'>" + String(stat_lastGameShots) + "</div><div>Last Game Shots</div></div>";
  h += "</div></div>";

  // BUTTONS TO CONFIG PAGES
  h += "<div style='display:flex;gap:10px;justify-content:center;margin-bottom:20px;margin-top:20px;'>";
  h += "<a href='/sounds' style='flex:1;'><button style='background:#ff00ff;font-weight:bold;font-size:1.1em;padding:12px;'>🎵 SOUNDS</button></a>";
  h += "<a href='/colors' style='flex:1;'><button style='background:#00ffff;color:#000;font-weight:bold;font-size:1.1em;padding:12px;'>🎨 COLOR CONFIG</button></a>";
  h += "</div>";

  String pName = (currentProfilePrefix == "def_") ? "Standard" : ((currentProfilePrefix == "kid_") ? "Kids" : "Pro");
  h += "<div class='sec'><h3>Profile Management</h3>Current Profile: <b>" + pName + "</b><br><form action='/loadprofile' method='POST' style='display:flex;gap:5px;margin-top:5px;'><select name='profile'><option value='def' " + String(currentProfilePrefix == "def_" ? "selected" : "") + ">Standard</option><option value='kid' " + String(currentProfilePrefix == "kid_" ? "selected" : "") + ">Kids</option><option value='pro' " + String(currentProfilePrefix == "pro_" ? "selected" : "") + ">Pro/Party</option></select><button type='submit'>Load Profile</button></form></div>";

  h += "<form action='/save' method='POST'><div class='sec'><h3>Hardware & General</h3>Start Level: <input type='number' name='startlvl' min='1' max='10' value='" + String(config_start_level) + "'><br>";
  h += "Total LEDs: <input id='ledCount' type='number' name='leds' value='" + String(config_num_leds) + "' oninput='updateCalc()'><br>";
  h += "<label>Sacrificial LED: <input type='checkbox' name='sac_led' value='1' " + String(config_sacrifice_led ? "checked" : "") + " style='width:auto;'></label><br>";
  h += "<label>Homebase Size: <input type='number' name='hb_size' min='1' max='5' value='" + String(config_homebase_size) + "'></label><br>";
  h += "<label>Player Shot Speed: <span id='shotVal'>" + String(config_shot_speed_pct) + "%</span></label><input type='range' name='shot_spd' min='50' max='150' value='" + String(config_shot_speed_pct) + "' oninput=\"document.getElementById('shotVal').innerText = this.value + '%';\"><br>";
  h += "<label>Default Brightness: <span id='brightVal'>" + String(config_brightness_pct) + "%</span></label><input id='brightness' type='range' name='bright' min='10' max='100' value='" + String(config_brightness_pct) + "' oninput='updateCalc()'><div style='margin-top:5px;'>Est. Current: <span id='ampValue' class='val-highlight'>0.00 A</span></div>";

  h += "<div style='margin-top:10px;border-top:1px dashed #555;padding-top:10px;'>";
  h += "<label>Sound Enabled: <input type='checkbox' name='snd_on' value='1' " + String(config_sound_on ? "checked" : "") + " style='width:auto;'></label><br>";
  h += "<label>Master Volume: <span id='volVal'>" + String(config_volume_pct) + "%</span></label><input type='range' name='vol' min='0' max='100' value='" + String(config_volume_pct) + "' oninput=\"document.getElementById('volVal').innerText = this.value + '%';\">";
  h += "</div>";

  h += "<div id='warning-box'></div></div>";
  h += "<div class='sec'><h3>Network</h3><label>WiFi SSID:</label><input type='text' name='ssid' value='" + config_ssid + "' placeholder='WiFi Name'>Password: <input type='password' name='pass' value='" + config_pass + "'><br><input type='checkbox' id='chkStatic' name='static_ip' value='1' onchange='toggleIP()' " + String(config_static_ip ? "checked" : "") + " style='width:auto;'> Static IP<br><div id='ipsettings' style='display:none;margin-top:10px;'>IP: <input name='ip' value='" + config_ip + "'>Gateway: <input name='gw' value='" + config_gateway + "'>Subnet: <input name='sn' value='" + config_subnet + "'>DNS: <input name='dns' value='" + config_dns + "'></div></div>";
  h += "<div class='sec'><h3>Level Configuration</h3><table><thead><tr><th>Lvl</th><th>Speed</th><th>Len</th><th>Boss?</th></tr></thead><tbody>";
  for (int i = 1; i <= 10; i++) { h += "<tr><td data-label='Level'>" + String(i) + "</td><td data-label='Speed'><input name='lspd" + String(i) + "' value='" + String(levels[i].speed) + "'></td><td data-label='Len/Type'><input name='llen" + String(i) + "' value='" + String(levels[i].length) + "'></td><td data-label='Boss'><select name='lboss" + String(i) + "'><option value='0' " + String(levels[i].bossType == 0 ? "selected" : "") + ">-</option><option value='2' " + String(levels[i].bossType == 2 ? "selected" : "") + ">Masterblaster</option><option value='1' " + String(levels[i].bossType == 1 ? "selected" : "") + ">The Tank</option><option value='3' " + String(levels[i].bossType == 3 ? "selected" : "") + ">RGB Overlord</option></select></td></tr>"; }
  h += "</tbody></table></div>";

  h += "<div class='sec'><h3>Boss Configuration</h3>";
  h += "<div class='b-card'><div class='b-head'>Masterblaster (Boss 1)</div><div class='form-grid'>";
  h += "<div class='f-item'><span class='lbl'>Move Speed</span><input name='b2mv' value='" + String(boss2Cfg.moveSpeed) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Shot Speed</span><input name='b2ss' value='" + String(boss2Cfg.shotSpeed) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>HP/LED</span><input name='b2hp' value='" + String(boss2Cfg.hpPerLed) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Reload (0.1s)</span><input name='b2fr' value='" + String(boss2Cfg.shotFreq) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Mark 1 (%)</span><input name='b2m1' value='" + String(boss2Cfg.m1) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Mark 2 (%)</span><input name='b2m2' value='" + String(boss2Cfg.m2) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Mark 3 (%)</span><input name='b2m3' value='" + String(boss2Cfg.m3) + "'></div>";
  h += "</div></div>";

  h += "<div class='b-card'><div class='b-head'>The Tank (Boss 2)</div><div class='form-grid'>";
  h += "<div class='f-item'><span class='lbl'>Move Speed</span><input name='b1mv' value='" + String(boss1Cfg.moveSpeed) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Shot Speed</span><input name='b1ss' value='" + String(boss1Cfg.shotSpeed) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>HP/LED</span><input name='b1hp' value='" + String(boss1Cfg.hpPerLed) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Shot Freq</span><input name='b1fr' value='" + String(boss1Cfg.shotFreq) + "'></div>";
  h += "</div></div>";

  h += "<div class='b-card'><div class='b-head'>RGB Overlord (Boss 3)</div><div class='form-grid'>";
  h += "<div class='f-item'><span class='lbl'>Move Speed</span><input name='b3mv' value='" + String(boss3Cfg.moveSpeed) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>HP/LED</span><input name='b3hp' value='" + String(boss3Cfg.hpPerLed) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Shot Freq</span><input name='b3fr' value='" + String(boss3Cfg.shotFreq) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Burst Count</span><input name='b3bc' value='" + String(boss3Cfg.burstCount) + "'></div>";
  h += "</div></div>";

  h += "</div>";

  h += "<br><input type='submit' value='SAVE SETTINGS' style='width:100%;background:#009900;padding:15px;font-size:1.2em;cursor:pointer;font-weight:bold;'></form>";
  h += "<div style='margin-top:20px;text-align:center;'><a href='/update'><button style='background:#0044cc;padding:10px;font-size:1em;'>⬇ FIRMWARE UPDATE ⬇</button></a></div>";

  //  NEW – Continue to next level (GPIO‑27) ---------------------------------
  h += "<div style='margin-top:20px; text-align:center;'>";
  h += "  <form action='/next' method='POST'>";  // <-- POST to the /next endpoint we added in setup()
  h += "    <button type='submit' style='width:100%;background:#0066ff;padding:12px;font-size:1.1em;'>▶ Continue to Next Level</button>";
  h += "  </form>";
  h += "</div>";

  h += "<br><br><form action='/reset' method='POST' onsubmit='return confirmReset()'><button type='submit' style='background:#990000;padding:10px;'>FACTORY RESET (Clear Scores)</button></form>";
  h += "<div class='credits'><a href='https://paypal.me/WeisWernau' target='_blank'>paypal.me/WeisWernau</a><br><br><i>\"I don't need your money. But if I can buy my wife a bouquet of flowers, the chance increases that I can publish more funny projects - every married man knows what I'm talking about.\"</i></div>";
  h += "</body></html>";
  return h;
}


/* ---------------------------- enableWiFi ----------------------------------------
   Starts the ESP32 in AP+STA mode, connects to the configured Wi Fi network (if any),
   launches the captive AP “ESP RGB INVADERS”, and starts the HTTP server.
   -------------------------------------------------------------------------------- */
void enableWiFi() {
  FastLED.clear();
  FastLED.show();
  WiFi.mode(WIFI_AP_STA);
  if (config_ssid != "") { WiFi.begin(config_ssid.c_str(), config_pass.c_str()); }
  if (config_static_ip && config_ip.length() > 0) {
    IPAddress ip, gw, sn, dns;
    if (ip.fromString(config_ip) && gw.fromString(config_gateway) && sn.fromString(config_subnet)) {
      if (config_dns.length() > 0) dns.fromString(config_dns);
      else dns.fromString("8.8.8.8");
      WiFi.config(ip, gw, sn, dns);
    }
  }
  WiFi.softAP("ESP-RGB-INVADERS", "12345678");
  server->begin();
}


// ==========================================================================
// 8. SETUP
// ==========================================================================
void setup() {
  // -----------------------------
  // 1. SERIAL + BASIC GPIO
  // -----------------------------
  Serial.begin(115200);
  Serial.println();
  Serial.println("SETUP: start");

  pinMode(PIN_BTN_BLUE, INPUT_PULLUP);
  pinMode(PIN_BTN_RED, INPUT_PULLUP);
  pinMode(PIN_BTN_GREEN, INPUT_PULLUP);
  pinMode(PIN_BTN_WHITE, INPUT_PULLUP);
  pinMode(PIN_BTN_CONTINUE, INPUT_PULLUP);

  // -------------------------------------------------
  // Initialise the hint LEDs (all OFF)
  // -------------------------------------------------
  pinMode(PIN_HINT_RED, OUTPUT);
  pinMode(PIN_HINT_BLUE, OUTPUT);
  pinMode(PIN_HINT_GREEN, OUTPUT);
  digitalWrite(PIN_HINT_RED, LOW);
  digitalWrite(PIN_HINT_BLUE, LOW);
  digitalWrite(PIN_HINT_GREEN, LOW);

  pinMode(PIN_GPIO19_LED, OUTPUT);
  digitalWrite(PIN_GPIO19_LED, LOW);

  // -----------------------------
  // 2. CONFIG + PREFERENCES
  // -----------------------------
  setupDefaultConfig();

  preferences.begin("game", true);
  int storedVer = preferences.getInt("version", 0);
  preferences.end();

  if (storedVer < CONFIG_VERSION) {
    performFactoryReset();
    ESP.restart();
  }

  preferences.begin("game", true);
  currentProfilePrefix = preferences.getString("act_prof", "def_");
  preferences.end();

  loadConfig(currentProfilePrefix);

  // --- SANITIZE LED COUNT FROM UI ---
  if (config_num_leds < 30) config_num_leds = 30;
  if (config_num_leds > MAX_LEDS) config_num_leds = MAX_LEDS;

  loadHighscores();
  loadSounds();
  loadColors();

  // -----------------------------
  // 3. FASTLED
  // -----------------------------
  Serial.println("SETUP: FastLED");

  FastLED.addLeds<LED_TYPE, PIN_LED_DATA, COLOR_ORDER>(leds, config_num_leds + 1);
  FastLED.setBrightness(map(config_brightness_pct, 10, 100, 25, 255));
  FastLED.setDither(0);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2500);

  if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
  FastLED.show();

  // -----------------------------
  // 4. GAME STARTUP
  // -----------------------------
  Serial.println("SETUP: 4. Game Startup");
  startLevelIntro(config_start_level);

  FastLED.clear();
  for (int i = 0; i <= config_num_leds; i += 2)
    leds[i + ledStartOffset] = CRGB::Green;
  FastLED.show();
  delay(500);

  // -----------------------------
  // 5. WIFI (MUST COME BEFORE SERVER)
  // -----------------------------
  Serial.println("SETUP: 5. WiFi begin");

  WiFi.mode(WIFI_STA);
  WiFi.begin("Kaywinnet", "806194edb8");

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  // -----------------------------
  // 6. CREATE WEB SERVER
  // -----------------------------
  Serial.println("SETUP: Creating WebServer");

  server = new WebServer(80);

  // ROUTES
  server->on("/", []() {
    server->send(200, "text/html", getHTML());
  });

  server->on("/save", handleSave);
  server->on("/loadprofile", handleProfileSwitch);
  server->on("/reset", handleReset);

  server->on("/sounds", []() {
    server->send(200, "text/html", getSoundHTML());
  });

  server->on("/colors", []() {
    server->send(200, "text/html", getColorHTML());
  });

  server->on("/savecolors", handleSaveColors);

  server->on("/update", HTTP_GET, []() {
    server->send(200, "text/html", getUpdateHTML());
  });

  server->on(
    "/update", HTTP_POST,
    []() {
      server->send(200, "text/plain",
                   (Update.hasError()) ? "UPDATE FAILED" : "UPDATE SUCCESS! RESTARTING...");
      ESP.restart();
    },
    []() {
      HTTPUpload &upload = server->upload();
      if (upload.status == UPLOAD_FILE_START) {
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
          Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) Serial.printf("Update Success: %u\n", upload.totalSize);
        else Update.printError(Serial);
      }
    });

  server->on("/next", HTTP_POST, handleContinue);

  // -----------------------------
  // 7. START WEB SERVER
  // -----------------------------
  server->begin();
  Serial.println("Web server started");

  // -----------------------------
  // 8. START UDP
  // -----------------------------
  udp.begin(4210);  // any local port is fine
  Serial.println("UDP ready");

  Serial.println("SETUP: end");
}



// ============================================================================
// loop
// ============================================================================
void loop() {
  unsigned long now = millis();
  if (now - lastLoopTime < FRAME_DELAY) return;
  lastLoopTime = now;

  /* --------------------------------------------------------------
     Always handle web requests (the tiny config UI)
     -------------------------------------------------------------- */
  server->handleClient();

  /* --------------------------------------------------------------
     WHITE button (GPIO 32) – Reset / Continue handling
     -------------------------------------------------------------- */
  if (digitalRead(PIN_BTN_WHITE) == LOW) {  // button pressed
    if (!btnWhiteHeld) {                    // first edge
      btnWhiteHeld = true;
      btnWhitePressTime = now;
    } else {
      // Long‑press (>3 s) → Wi‑Fi configuration mode (unchanged)
      if (now - btnWhitePressTime > 3000) {
        FastLED.clear();
        if ((now / 250) % 2 == 0) {
          for (int i = 0; i < config_num_leds; i += 2)
            leds[i + ledStartOffset] = CRGB::Blue;
        }
        FastLED.show();
        return;  // stay in Wi‑Fi mode until button is released
      }
    }
  } else {               // button released
    if (btnWhiteHeld) {  // we just let go
      unsigned long pressLen = now - btnWhitePressTime;
      btnWhiteHeld = false;

      // ---- long press → Wi‑Fi (safety net) ----
      if (pressLen > 3000) {
        wifiMode = true;
        enableWiFi();  // start AP + web‑UI
        return;
      }

      // ---- short press → **full game reset** ----
      abortAndResetGame();  // true full reset
      return;               // stop processing the rest of loop()
    }
  }


  /* --------------------------------------------------------------
     CONTINUE button (GPIO 27) – go to the next level (only when a
     level has just been won)
     -------------------------------------------------------------- */
  if (digitalRead(PIN_BTN_CONTINUE) == LOW) {  // pressed
    if (!btnContinueHeld) {                    // first edge
      btnContinueHeld = true;
      btnContinuePressTime = now;
    }
  } else {                  // released
    if (btnContinueHeld) {  // we just let go
      unsigned long holdTime = now - btnContinuePressTime;
      btnContinueHeld = false;

      if (levelJustFinished && holdTime < 1000) {  // short tap while win screen
        continueToNextLevel();                     // start next level, keep score
      }
    }
  }



  /* --------------------------------------------------------------
      Blink the LED (LED0) 2 Hz  **and** GPIO 19 in sync while we are waiting on the
      level‑completed screen
     -------------------------------------------------------------- */
  if (levelJustFinished) {
    static unsigned long lastBlink = 0;  // already existed for the strip LED
    if (now - lastBlink >= 250) {        // 250 ms → 2 Hz
      lastBlink = now;

      // ----- toggle the NeoPixel sacrificial LED -----
      if (config_sacrifice_led) {
        leds[0] = (leds[0] == CRGB::Black) ? CRGB(20, 0, 0) : CRGB::Black;
        FastLED.show();
      }

      // ----- toggle the external GPIO‑19 LED in the exact same instant -----
      gpio19State = !gpio19State;  // invert state
      digitalWrite(PIN_GPIO19_LED,
                   gpio19State ? HIGH : LOW);  // HIGH = LED ON
    }
  }



  /* --------------------------------------------------------------
     Quick‑return for the various non‑playing states
     -------------------------------------------------------------- */
  if (currentState == STATE_LEVEL_COMPLETED) {
    updateLevelCompletedAnim();
    return;
  }
  if (currentState == STATE_BASE_DESTROYED) {
    updateBaseDestroyedAnim();
    return;
  }
  if (currentState == STATE_GAME_FINISHED) {
    for (int i = 0; i < config_num_leds; i++)
      leds[i + ledStartOffset] = CHSV((now / 10) + (i * 5), 255, 255);
    if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
    FastLED.show();
    return;
  }
  if (currentState == STATE_GAMEOVER) {
    for (int i = 0; i < config_num_leds; i++)
      leds[i + ledStartOffset] = CRGB::Red;
    if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
    FastLED.show();
    return;
  }
  if (currentState == STATE_INTRO) {
    updateLevelIntro();
    return;
  }
  if (currentState == STATE_MENU) {
    drawMenu();
    clearHintLEDs();  // force all hint LEDs OFF while in the menu
    /* (the menu uses the white button to start the next level) */
    if (digitalRead(PIN_BTN_WHITE) == LOW && !btnWhiteHeld) {
      btnWhiteHeld = true;
      btnWhitePressTime = now;
    }
    if (btnWhiteHeld && digitalRead(PIN_BTN_WHITE) == HIGH) {
      btnWhiteHeld = false;
      if (levelJustFinished) continueToNextLevel();
    }
    return;
  }

  /* --------------------------------------------------------------
     PLAYING / BOSS PLAYING – main game logic
     -------------------------------------------------------------- */
  if (currentState == STATE_PLAYING || currentState == STATE_BOSS_PLAYING) {
    /* ------------------  INPUT ------------------- */
    bool b = (digitalRead(PIN_BTN_BLUE) == LOW);
    bool r = (digitalRead(PIN_BTN_RED) == LOW);
    bool g = (digitalRead(PIN_BTN_GREEN) == LOW);
    bool isAnyBtnPressed = (b || r || g);

    /* ---------------------------------------------------------
       NEW PART – immediately turn OFF the hint LEDs whenever
       **any** colour button is pressed (even if the colour is wrong)
       ---------------------------------------------------------- */
    bool buttonPressedThisFrame = isAnyBtnPressed;  // remember for later
    if (buttonPressedThisFrame) {
      // force all hint pins LOW right now
      digitalWrite(PIN_HINT_RED, LOW);
      digitalWrite(PIN_HINT_BLUE, LOW);
      digitalWrite(PIN_HINT_GREEN, LOW);
      // also clear the pending‑hint flag so the later logic cannot re‑turn it on
      hintPending = false;
    }

    if (!isAnyBtnPressed) {
      buttonsReleased = true;
      isWaitingForCombo = false;
    }

    /* ------------------  INPUT HANDLING ------------------ */
    if (currentBossType == 3) {  // RGB Overlord – combo mode
      if (isAnyBtnPressed && buttonsReleased && !isWaitingForCombo && (now - lastFireTime > FIRE_COOLDOWN)) {
        isWaitingForCombo = true;
        comboTimer = now;
      }
      if (isWaitingForCombo && (now - comboTimer >= INPUT_BUFFER_MS)) {
        int c = 0;
        b = (digitalRead(PIN_BTN_BLUE) == LOW);
        r = (digitalRead(PIN_BTN_RED) == LOW);
        g = (digitalRead(PIN_BTN_GREEN) == LOW);
        if (r && g && b) c = 7;
        else if (r && g) c = 4;
        else if (r && b) c = 5;
        else if (g && b) c = 6;
        else if (b) c = 1;
        else if (r) c = 2;
        else if (g) c = 3;
        if (c) {
          shots.push_back({ 0.0, c });
          stat_totalShots++;
          stat_lastGameShots++;
          lastFireTime = now;
          playShotSound(c);
        }
        buttonsReleased = false;
        isWaitingForCombo = false;
      }
    } else {  // normal enemies / other bosses
      if (isAnyBtnPressed && buttonsReleased && (now - lastFireTime > FIRE_COOLDOWN)) {
        int c = 0;
        if (b) c = 1;
        else if (r) c = 2;
        else if (g) c = 3;
        if (c) {
          shots.push_back({ 0.0, c });
          stat_totalShots++;
          stat_lastGameShots++;
          lastFireTime = now;
          playShotSound(c);
        }
        buttonsReleased = false;
      }
    }

    /* ------------------  SHOT MOVEMENT ------------------- */
    float moveStep = (float)config_shot_speed_pct / 60.0;
    moveStep = moveStep * 0.6;
    if (moveStep < 0.2) moveStep = 0.2;

    for (int i = shots.size() - 1; i >= 0; i--) {
      shots[i].position += moveStep;
      bool remove = false;

      /* ===  HIT DETECTION  === */
      if (currentState == STATE_PLAYING) {  // normal wave
        if (shots[i].position >= enemyFrontIndex && !enemies.empty()) {
          if (shots[i].color == enemies[0].color) {  // **correct hit**
            enemies.erase(enemies.begin());
            stat_totalKills++;
            enemyFrontIndex += 1.0;
            flashPixel((int)shots[i].position);
            remove = true;
            checkWinCondition();
            hitJustOccurred = true;  // tell hint logic
            playSound(EVT_HIT_SUCCESS);
          } else {  // wrong colour → penalty
            enemies.insert(enemies.begin(),
                           { shots[i].color, 0.0 });
            enemyFrontIndex -= 1.0;
            remove = true;
            playSound(EVT_MISTAKE);
          }
        }
      } else if (currentState == STATE_BOSS_PLAYING) {  // boss fight
        /* ----- boss projectiles ----- */
        for (int p = 0; p < bossProjectiles.size(); p++) {
          if (shots[i].position >= bossProjectiles[p].pos) {
            if (shots[i].color == bossProjectiles[p].color) {
              bossProjectiles.erase(bossProjectiles.begin() + p);
              flashPixel((int)shots[i].position);
            }
            remove = true;
            break;
          }
        }

        /* ----- boss segment collision ----- */
        if (!remove && shots[i].position >= enemyFrontIndex && !bossSegments.empty()) {
          int hitIndex = (int)(shots[i].position - enemyFrontIndex);
          if (hitIndex >= 0 && hitIndex < bossSegments.size()) {
            bool vulnerable = false;
            if (currentBossType == 1) vulnerable = true;
            else if (currentBossType == 2) {
              if (boss2State == B2_MOVE && bossSegments[hitIndex].active) vulnerable = true;
            } else if (currentBossType == 3) {
              if (boss3State != B3_PHASE_CHANGE) vulnerable = true;
            }

            if (vulnerable && shots[i].color == bossSegments[hitIndex].color) {
              flashPixel((int)shots[i].position);
              bossSegments[hitIndex].hp--;
              if (bossSegments[hitIndex].hp <= 0) {
                bossSegments.erase(bossSegments.begin() + hitIndex);
                stat_totalKills++;
                if (hitIndex == 0) enemyFrontIndex += 1.0;
                playSound(EVT_HIT_SUCCESS);
                hitJustOccurred = true;  // tell hint logic
              }
              checkWinCondition();
            }
            remove = true;
          }
        }
      }

      if (shots[i].position >= config_num_leds) remove = true;
      if (remove) shots.erase(shots.begin() + i);
    }

    /* ------------------  ENEMY / BOSS MOVEMENT ------------------- */
    if (currentState == STATE_PLAYING) {
      float enemySpeed = (float)levels[currentLevel].speed;
      float eStep = enemySpeed / 60.0;
      enemyFrontIndex -= eStep;
      if (enemyFrontIndex <= config_homebase_size) triggerBaseDestruction();
    } else {  // STATE_BOSS_PLAYING
      int pSpeed = 60;
      if (currentBossType == 1) pSpeed = boss1Cfg.shotSpeed;
      if (currentBossType == 2) pSpeed = boss2Cfg.shotSpeed;
      moveBossProjectiles((float)pSpeed);

      if (currentBossType == 1) {  // Masterblaster
        float bStep = (float)boss1Cfg.moveSpeed / 60.0;
        enemyFrontIndex -= bStep;
        if (enemyFrontIndex <= config_homebase_size) triggerBaseDestruction();
        if (now - bossActionTimer > (boss1Cfg.shotFreq * 100)) {
          bossActionTimer = now;
          int shotColor = 0;
          int frontColor = (bossSegments.size() ? bossSegments[0].color : 0);
          if (random(100) < 20 && frontColor > 0) shotColor = frontColor;
          else {
            do { shotColor = random(1, 4); } while (shotColor == frontColor && frontColor > 0);
          }
          bossProjectiles.push_back({ enemyFrontIndex, shotColor });
        }
      } else if (currentBossType == 2) {  // The Tank
        if (boss2State == B2_MOVE) {
          float bStep = (float)boss2Cfg.moveSpeed / 60.0;
          enemyFrontIndex -= bStep;
          if (boss2Section < 3 && enemyFrontIndex <= markerPos[boss2Section]) {
            boss2State = B2_CHARGE;
            bossActionTimer = now;
          }
          if (enemyFrontIndex <= config_homebase_size) triggerBaseDestruction();
        } else if (boss2State == B2_CHARGE) {
          if (now - bossActionTimer < (boss2Cfg.shotFreq * 100)) {
            if (now % 100 < 20) boss2LockedColor = random(1, 4);
          } else {
            boss2State = B2_SHOOT;
            boss2ShotsFired = 0;
            bossActionTimer = now;
            int startRange = 0, endRange = 0;
            if (boss2Section == 0) {
              startRange = 0;
              endRange = 2;
            } else if (boss2Section == 1) {
              startRange = 0;
              endRange = 5;
            } else {
              startRange = 0;
              endRange = 8;
            }
            for (auto &seg : bossSegments)
              if (seg.originalIndex >= startRange && seg.originalIndex <= endRange)
                seg.color = boss2LockedColor;
          }
        } else if (boss2State == B2_SHOOT) {
          if (now - bossActionTimer > 150) {
            bossActionTimer = now;
            bossProjectiles.push_back({ enemyFrontIndex, boss2LockedColor });
            boss2ShotsFired++;
            if (boss2ShotsFired >= 10) {
              int startRange = 0, endRange = 0;
              if (boss2Section == 0) {
                startRange = 0;
                endRange = 2;
              } else if (boss2Section == 1) {
                startRange = 3;
                endRange = 5;
              } else {
                startRange = 0;
                endRange = 8;
              }
              for (auto &seg : bossSegments)
                if (seg.originalIndex >= startRange && seg.originalIndex <= endRange)
                  seg.active = true;
              boss2State = B2_MOVE;
              boss2Section++;
            }
          }
        }
      } else if (currentBossType == 3) {  // RGB Overlord
        float safeFireLimit = (config_num_leds > 180) ? 70.0 : (float)(config_homebase_size + 5);

        if (boss3State == B3_MOVE && boss3PhaseIndex < 2 && enemyFrontIndex <= boss3Markers[boss3PhaseIndex]) {
          boss3State = B3_PHASE_CHANGE;
          bossActionTimer = now;
        }

        if (boss3State == B3_MOVE) {
          float bStep = (float)boss3Cfg.moveSpeed / 60.0;
          enemyFrontIndex -= bStep;
          if (enemyFrontIndex <= config_homebase_size) triggerBaseDestruction();
          if (enemyFrontIndex > safeFireLimit && boss3Cfg.shotFreq > 0 && (now - bossActionTimer > (boss3Cfg.shotFreq * 100))) {
            bossActionTimer = now;
            bossProjectiles.push_back({ enemyFrontIndex, random(1, 4) });
          }
        } else if (boss3State == B3_PHASE_CHANGE) {
          if (now - bossActionTimer > 4000) {
            boss3State = B3_BURST;
            boss3BurstCounter = 0;
            bossActionTimer = now;
            for (auto &seg : bossSegments) seg.color = random(4, 8);
            boss3PhaseIndex++;
          }
        } else if (boss3State == B3_BURST) {
          if (now - bossActionTimer > 200) {
            bossActionTimer = now;
            if (enemyFrontIndex > safeFireLimit) {
              bossProjectiles.push_back({ enemyFrontIndex, random(1, 8) });
            }
            boss3BurstCounter++;
            if (boss3BurstCounter >= boss3Cfg.burstCount) {
              boss3State = B3_WAIT;
              bossActionTimer = now;
            }
          }
        } else if (boss3State == B3_WAIT) {
          if (now - bossActionTimer > 2000) {
            boss3State = B3_MOVE;
            bossActionTimer = now;
          }
        }
      }
    }

    /* --------------------------------------------------------------
       DRAW the LED strip (unchanged)
       -------------------------------------------------------------- */
    FastLED.clear();

    // ---- optional visual markers for some bosses (unchanged) ----
    if (currentState == STATE_BOSS_PLAYING) {
      if (currentBossType == 2) {
        for (int i = 0; i < 3; i++)
          if (markerPos[i] < enemyFrontIndex)
            leds[markerPos[i] + ledStartOffset] = CRGB(50, 0, 0);
      } else if (currentBossType == 3) {
        if (boss3PhaseIndex <= 0) {
          leds[boss3Markers[0] + ledStartOffset] = CRGB(50, 0, 0);
          leds[boss3Markers[0] + ledStartOffset + 1] = CRGB(50, 0, 0);
        }
        if (boss3PhaseIndex <= 1) {
          leds[boss3Markers[1] + ledStartOffset] = CRGB(50, 0, 0);
          leds[boss3Markers[1] + ledStartOffset + 1] = CRGB(50, 0, 0);
        }
      }
    }

    // ---- normal enemies (wave) ----
    if (currentState == STATE_PLAYING) {
      for (size_t i = 0; i < enemies.size(); i++) {
        float pos = enemyFrontIndex + (float)i;
        drawCrispPixel(pos, getColor(enemies[i].color));
      }
    }
    // ---- boss segments ----
    else if (currentState == STATE_BOSS_PLAYING) {
      for (size_t i = 0; i < bossSegments.size(); i++) {
        float pos = enemyFrontIndex + (float)i;
        if (pos >= 0 && pos < config_num_leds) {
          CRGB c = getColor(bossSegments[i].color);
          if (currentBossType == 2) {  // The Tank
            c = col_cb;
            if (boss2State == B2_MOVE) {
              if (bossSegments[i].active) {
                c = getColor(bossSegments[i].color);
                if ((millis() / 100) % 2 == 0) c = CRGB::Black;
              }
            } else if (boss2State == B2_CHARGE || boss2State == B2_SHOOT) {
              int oid = bossSegments[i].originalIndex;
              bool highlight = false;
              if (boss2Section == 0 && oid >= 0 && oid <= 2) highlight = true;
              else if (boss2Section == 1 && oid >= 0 && oid <= 5) highlight = true;
              else if (boss2Section >= 2) highlight = true;
              if (highlight) c = getColor(boss2LockedColor);
            }
          } else if (currentBossType == 3 && boss3State == B3_PHASE_CHANGE) {
            c = CRGB::White;
          }
          drawCrispPixel(pos, c);
        }
      }
      // boss projectiles
      for (auto &p : bossProjectiles)
        drawCrispPixel(p.pos, getColor(p.color));
    }

    // ---- player shots ----
    for (auto &s : shots)
      drawCrispPixel(s.position, getColor(s.color));

    // ---- home‑base (white) ----
    for (int i = 0; i < config_homebase_size; i++)
      leds[i + ledStartOffset] = CRGB::White;

    // ---- sacrificial LED (red) ----
    if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);

    /* --------------------------------------------------------------
       **HINT‑LED UPDATE** – runs after all game logic but
       **before** the strip is finally shown.
       -------------------------------------------------------------- */
    int leadColourNow = -1;
    if (currentState == STATE_PLAYING && !enemies.empty())
      leadColourNow = enemies.front().color;  // 1 = blue, 2 = red, 3 = green
    else if (currentState == STATE_BOSS_PLAYING && !bossSegments.empty())
      leadColourNow = bossSegments.front().color;  // may be 1‑6 (mix colours)

    bool showHint = false;

    // **IMPORTANT:** if the player pressed any button this frame we *must*
    // keep the hint off, regardless of colour changes.
    if (!buttonPressedThisFrame) {
      if (leadColourNow != -1) {
        if (leadColourNow != lastLeadColour) {  // colour changed
          showHint = true;
        } else if (hitJustOccurred && leadColourNow == lastLeadColour) {  // we just killed it
          showHint = true;
        } else {
          showHint = hintPending;  // keep previous state
        }
      }
    } else {
      // button was pressed – keep hint off
      showHint = false;
    }

    if (showHint) {
      digitalWrite(PIN_HINT_RED, (leadColourNow == 2) ? HIGH : LOW);
      digitalWrite(PIN_HINT_BLUE, (leadColourNow == 1) ? HIGH : LOW);
      digitalWrite(PIN_HINT_GREEN, (leadColourNow == 3) ? HIGH : LOW);
    } else {
      digitalWrite(PIN_HINT_RED, LOW);
      digitalWrite(PIN_HINT_BLUE, LOW);
      digitalWrite(PIN_HINT_GREEN, LOW);
    }

    // remember for the next frame
    hintPending = showHint;
    lastLeadColour = leadColourNow;
    hitJustOccurred = false;  // reset for next loop

    // --------------------------------------------------------------
    // make sure GPIO19 is off when the level‑finished flag is cleared
    // --------------------------------------------------------------
    if (!levelJustFinished && gpio19State) {
      gpio19State = false;
      digitalWrite(PIN_GPIO19_LED, LOW);
    }

    FastLED.show();
  }
}
