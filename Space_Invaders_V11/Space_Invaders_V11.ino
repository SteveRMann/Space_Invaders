/*
==========================================================================
PROJECT: Space Invders - 

Board Settings
Board: ESP32 Dev Module (ESP32-WROOM-32 DevKit, 4 MB flash)
Flash Size: 4MB
Partition: Default
PSRAM: Disabled

CORE VERSION: 2.0.17 (Required??)
V1 This version strips the sound and 12s code.
V2 adds a "continue" button to go to the next level.
V3 adds "hint" LEDS, renamed White Button to Reset Button
V4 Adds UDP commands to play sounds
V5 Restores web page to set WiFi Credentials
V6 Adds debounce to the RGB buttons.
V7 Replace UDP Sound Functions with tone()
   Add missed sprites to the END of the enemy array.
V8 Fixed upload problem, added hint option.
V9 Removed game statistics
V10 Removed scoring
V11 Removed all web code: WiFi, web server, web OTA, UDP, and NVS (Preferences)
    storage. All settings now live in the CONFIG section below - edit and re-upload.

==========================================================================
*/

#define VERSION 11.0  // No web/WiFi. Halloween: hint LEDs on all the time.
                      // hintLEDsOFF()

bool config_hint_always_on = true;  // Set to true to keep hints always ON

#include <vector>
#include "esp32-hal-ledc.h"  // For tone function



// --- LED CONFIGURATION ---
#include <FastLED.h>




// ============================================================================
// 1. DEFINITIONS & DATA TYPES
// ============================================================================
#define PIN_LED_DATA 15
#define MAX_LEDS 300
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


// status LED- follows the sacrificial‑LED blink
#define PIN_GPIO19_LED 19
static unsigned long lastBlink19 = 0;  // timestamp of the last toggle
static bool gpio19State = false;       // current level of GPIO19 (HIGH/LOW)

#define SPEAKER_PIN 4

#define FRAME_DELAY 16  // 16ms = approx. 60 FPS
#define RESULT_FLASH_MS 1000  // How long the green WIN / red LOSE strip stays lit before going black
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




// ============================================================================
// 2. GLOBAL VARIABLES
// ============================================================================
CRGB leds[MAX_LEDS + 1];  // +1 for the sacrificial LED


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

// ---------------------------------------------------------------------------
// CONFIG  - edit these and re-upload (nothing is stored on the ESP any more)
// ---------------------------------------------------------------------------
int config_num_leds = 478;        // Game LEDs (30..MAX_LEDS), not counting the sacrificial LED
int config_brightness_pct = 15;   // 10..100
int config_start_level = 1;       // 1..10
bool config_sacrifice_led = true; // true = LED 0 is a level-shifter "sacrificial" LED
int config_homebase_size = 3;     // LEDs in the home base
int config_shot_speed_pct = 100;  // Player shot speed, %
int ledStartOffset = 1;           // Set automatically from config_sacrifice_led

// AUDIO CONFIG
bool config_sound_on = true;
int config_volume_pct = 50;

// Difficulty profile: "def_" = Standard, "kid_" = Kids, "pro_" = Pro/Party
String currentProfilePrefix = "def_";

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


// V9 int currentScore = 0;
int levelMaxPossibleScore = 0;  // Used in the final score LED display
int levelAchievedScore = 0;     // Used in the final score LED display


// -----– colour‑hint state ------------------------------------------------
int lastLeadColour = -1;       // colour of the front entity on the previous frame
bool hintPending = false;      // true = we should light the appropriate hint LED
bool hitJustOccurred = false;  // set to true only when a correct shot destroyed the front enemy
// bool HINT = false;             // Show or not show hints. If false, LEDs always on.


//--------------- Button Debounce ------------------------
const uint16_t DEBOUNCE_MS = 30;
bool btnRawB, btnRawR, btnRawG;
bool btnStableB = false, btnStableR = false, btnStableG = false;
uint32_t lastChangeB = 0, lastChangeR = 0, lastChangeG = 0;


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
  /* V9
  currentScore = 0;
  levelMaxPossibleScore = 0;
  */
  levelAchievedScore = 0;  // Saved for the final score LED display


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
  // Reset the hint LEDs.
  // -------------------------------------------------
  hintLEDsOFF();



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

  // hintLEDsOFF();  // Reset the hint LEDs. This is normal
  hintLEDsON();  // Reset the hint LEDs. This is temporary for Halloween


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


/* ---------------- hintLEDsOFF() --------------------
   Turn the hint LEDs OFF
   ------------------------------------------------------- */
void hintLEDsOFF() {
  // For Halloween
  digitalWrite(PIN_HINT_RED, HIGH);
  digitalWrite(PIN_HINT_BLUE, HIGH);
  digitalWrite(PIN_HINT_GREEN, HIGH);
}


/* ---------------- hintLEDsON() --------------------
   Turn the hint LEDs ON
   ------------------------------------------------------- */
void hintLEDsON() {
  digitalWrite(PIN_HINT_RED, HIGH);
  digitalWrite(PIN_HINT_BLUE, HIGH);
  digitalWrite(PIN_HINT_GREEN, HIGH);
}


//--------------- Handle button debounce ----------------------
bool debounceButton(bool raw, bool &stable, uint32_t &lastChange, uint32_t now) {
  if (raw != stable) {
    lastChange = now;  // input changed — start debounce timer
  }

  if ((now - lastChange) > DEBOUNCE_MS) {
    stable = raw;  // input has been stable long enough
  }

  return stable;
}


// =========================================================================
// 4. AUDIO ENGINE
// =========================================================================


void playToneLocal(int freq, int duration_ms) {
  if (!config_sound_on || config_volume_pct == 0) return;

  // Map volume percentage to duty cycle (0-127)
  int volume = map(config_volume_pct, 0, 100, 0, 127);

  // Play the tone
  tone(SPEAKER_PIN, freq, duration_ms);
}


void playSequenceLocal(String seq) {
  if (seq.length() == 0) return;

  // Parse and play sequence
  int start = 0;
  int end = seq.indexOf(';');
  while (end != -1) {
    String pair = seq.substring(start, end);
    int comma = pair.indexOf(',');
    if (comma != -1) {
      int freq = pair.substring(0, comma).toInt();
      int dur = pair.substring(comma + 1).toInt();
      if (freq > 0 && dur > 0) {
        playToneLocal(freq, dur);
        // Small delay between tones to prevent overlap
        delay(5);
      }
    }
    start = end + 1;
    end = seq.indexOf(';', start);
  }

  // Last pair
  String pair = seq.substring(start);
  int comma = pair.indexOf(',');
  if (comma != -1) {
    int freq = pair.substring(0, comma).toInt();
    int dur = pair.substring(comma + 1).toInt();
    if (freq > 0 && dur > 0) {
      playToneLocal(freq, dur);
    }
  }
}



void playSound(SoundEvent evt) {
  Serial.print("playSound= ");
  Serial.print(evt);

  switch (evt) {
    case EVT_START:
      Serial.println(" Start");
      playSequenceLocal(cfg_snd_start);
      break;
    case EVT_WIN:
      Serial.println(" Win");
      playSequenceLocal(cfg_snd_win);
      break;
    case EVT_LOSE:
      Serial.println(" Lose");
      playSequenceLocal(cfg_snd_lose);
      break;
    case EVT_MISTAKE:
      Serial.println(" Mistake");
      playSequenceLocal(cfg_snd_mistake);
      break;
    case EVT_HIT_SUCCESS:
      Serial.println(" Hit");
      playSequenceLocal(cfg_snd_hit);
      break;
    default:
      Serial.println(" Unknown");
      playSequenceLocal("100,750");
  }
}


void playShotSound(int color) {
  Serial.print("playShotSound= ");
  Serial.print(color);

  switch (color) {
    case 1:  // Blue button
      Serial.println(" Blue btn");
      playSequenceLocal(cfg_snd_shot_b);
      break;
    case 2:  // Red button
      Serial.println(" Red btn");
      playSequenceLocal(cfg_snd_shot_r);
      break;
    case 3:  // Green button
      Serial.println(" Green btn");
      playSequenceLocal(cfg_snd_shot_g);
      break;
    default:
      Serial.println(" Unknown btn");
      playSequenceLocal("100,750");
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


void triggerBaseDestruction() {
  playSound(EVT_LOSE);
  currentState = STATE_BASE_DESTROYED;
  stateTimer = millis();
  hintLEDsOFF();
}



void checkWinCondition() {
  bool won = false;
  if (currentState == STATE_PLAYING && enemies.empty()) won = true;
  if (currentState == STATE_BOSS_PLAYING && bossSegments.empty()) won = true;
  if (won) {
    if (currentLevel >= 10) {
      playSound(EVT_WIN);
      currentState = STATE_GAME_FINISHED;
      //      hintLEDsOFF();  //Normal
      hintLEDsON();  // Reset the hint LEDs. This is temporary for Halloween
    } else {
      // ----- WIN THIS LEVEL ------------------
      playSound(EVT_WIN);
      currentState = STATE_LEVEL_COMPLETED;
      stateTimer = millis();

      // V9 FIX: Initialize score variables for the completion animation
      levelAchievedScore = 100;  // Or any value you want to display
      levelMaxPossibleScore = 100;

      levelJustFinished = true;  // <<< NEW – we are now waiting
    }
  }
}


void startLevelIntro(int level) {
  playSound(EVT_START);

  /* V9
  if (level == config_start_level) {
    currentScore = 0;
  }
  */

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
   Shows the “level cleared” animation: solid green for 1 s.
   NO score bar any more: after the green flash the game strip goes black
   while waiting for the Continue button. leds[0] (sacrificial LED) is left
   alone so the 2 Hz "waiting" blink in loop() keeps working.
   ----------------------------------------------------------------------- */
void updateLevelCompletedAnim() {
  unsigned long elapsed = millis() - stateTimer;
  if (elapsed < RESULT_FLASH_MS) {
    fill_solid(leds, config_num_leds + ledStartOffset, col_c3);
    if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
  } else {
    // Green "win" flash is over: blank the game strip
    fill_solid(&leds[ledStartOffset], config_num_leds, CRGB::Black);
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
    stateTimer = millis();  // start the red LOSE timeout
    hintLEDsOFF();
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
// 7. CONFIG (MUST BE BEFORE SETUP)
// ==========================================================================

/* ------------------------ loadColors ------------------------------------
   Converts the hex colour strings defined at the top of the sketch into
   the CRGB globals (col_c1, col_c2, …).
   ------------------------------------------------------------------------ */
void loadColors() {
  col_c1 = hexToCRGB(hex_c1);
  col_c2 = hexToCRGB(hex_c2);
  col_c3 = hexToCRGB(hex_c3);
  col_c4 = hexToCRGB(hex_c4);
  col_c5 = hexToCRGB(hex_c5);
  col_c6 = hexToCRGB(hex_c6);
  col_cw = hexToCRGB(hex_cw);
  col_cb = hexToCRGB(hex_cb);
}



/* ---------------------- loadSounds -----------------------------
   Loads the default sound strings defined at the top of the sketch and
   parses each into a Melody using melodyFromStr.
------------------------------------------------------------------ */
void loadSounds() {
  cfg_snd_start = DEF_SND_START;
  cfg_snd_win = DEF_SND_WIN;
  cfg_snd_lose = DEF_SND_LOSE;
  cfg_snd_mistake = DEF_SND_MISTAKE;
  cfg_snd_hit = DEF_SND_HIT;
  cfg_snd_shot_b = DEF_SND_SHOT_BLUE;
  cfg_snd_shot_r = DEF_SND_SHOT_RED;
  cfg_snd_shot_g = DEF_SND_SHOT_GREEN;
  cfg_snd_shot_w = DEF_SND_SHOT_WHITE;

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


// ==========================================================================
// 8. SETUP
// ==========================================================================
void setup() {
  // -----------------------------
  // 1. SERIAL + BASIC GPIO
  // -----------------------------
  currentState = STATE_MENU;  // Add this line to ensure we start in menu state

  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.print("=== SPACE INVADERS V");
  Serial.print(VERSION);
  Serial.println(" ===");
  Serial.printf("Free sketch space: %u bytes\n", ESP.getFreeSketchSpace());


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

  // Reset the hint LEDs.
  hintLEDsOFF();

  pinMode(PIN_GPIO19_LED, OUTPUT);
  digitalWrite(PIN_GPIO19_LED, LOW);

  // Initialize speaker pin (V7)
  pinMode(SPEAKER_PIN, OUTPUT);
  digitalWrite(SPEAKER_PIN, LOW);


  // -----------------------------
  // 2. CONFIG
  // -----------------------------
  applyProfileDefaults(currentProfilePrefix);  // level + boss tables
  ledStartOffset = config_sacrifice_led ? 1 : 0;

  // --- SANITIZE LED COUNT ---
  if (config_num_leds < 30) config_num_leds = 30;
  if (config_num_leds > MAX_LEDS) config_num_leds = MAX_LEDS;
  Serial.printf("LEDs: %d (+%d sacrificial)\n", config_num_leds, ledStartOffset);

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
  for (int i = 0; i < config_num_leds; i += 2)
    leds[i + ledStartOffset] = CRGB::Green;
  FastLED.show();
  delay(500);

  Serial.println("SETUP: end");
  hintLEDsON();  // Reset the hint LEDs. This is temporary for Halloween
}




// ============================================================================
// loop
// ============================================================================
void loop() {
  unsigned long now = millis();
  if (now - lastLoopTime < FRAME_DELAY) return;
  lastLoopTime = now;

  /* --------------------------------------------------------------
     WHITE button (GPIO 32) - Reset
     -------------------------------------------------------------- */
  if (digitalRead(PIN_BTN_WHITE) == LOW) {  // button pressed
    if (!btnWhiteHeld) {                    // first edge
      btnWhiteHeld = true;
      btnWhitePressTime = now;
    }
  } else {               // button released
    if (btnWhiteHeld) {  // we just let go
      btnWhiteHeld = false;

      // ---- any press -> full game reset ----
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
    // Red LOSE strip for RESULT_FLASH_MS (same as the green WIN flash), then black
    CRGB c = (now - stateTimer < RESULT_FLASH_MS) ? CRGB::Red : CRGB::Black;
    for (int i = 0; i < config_num_leds; i++)
      leds[i + ledStartOffset] = c;
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
    hintLEDsOFF();  // force all hint LEDs OFF while in the menu
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

    bool buttonPressedThisFrame = isAnyBtnPressed;  // remember for later
    if (buttonPressedThisFrame) {
      // Reset the hint LEDs.
      // hintLEDsOFF();

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

        uint32_t now = millis();

        btnRawB = (digitalRead(PIN_BTN_BLUE) == LOW);
        btnRawR = (digitalRead(PIN_BTN_RED) == LOW);
        btnRawG = (digitalRead(PIN_BTN_GREEN) == LOW);

        bool b = debounceButton(btnRawB, btnStableB, lastChangeB, now);
        bool r = debounceButton(btnRawR, btnStableR, lastChangeR, now);
        bool g = debounceButton(btnRawG, btnStableG, lastChangeG, now);

        if (r && g && b) c = 7;
        else if (r && g) c = 4;
        else if (r && b) c = 5;
        else if (g && b) c = 6;
        else if (b) c = 1;
        else if (r) c = 2;
        else if (g) c = 3;
        if (c) {
          shots.push_back({ 0.0, c });
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


      // ===  HIT DETECTION  === //
      if (currentState == STATE_PLAYING) {  // normal wave
        if (shots[i].position >= enemyFrontIndex && !enemies.empty()) {
          if (shots[i].color == enemies[0].color) {  // **correct hit**
            enemies.erase(enemies.begin());
            enemyFrontIndex += 1.0;
            flashPixel((int)shots[i].position);
            remove = true;
            checkWinCondition();
            hitJustOccurred = true;  // tell hint logic
            playSound(EVT_HIT_SUCCESS);
            /*
          } else {  // wrong colour → penalty.  Add players color to the START of the enemy array
            enemies.insert(enemies.begin(), { shots[i].color, 0.0 });
            enemyFrontIndex -= 1.0;
            remove = true;
            playSound(EVT_MISTAKE);
          }
          */
          } else {  // wrong colour → penalty. Add the player's color to the END of the enemy array
            enemies.push_back({ shots[i].color, 0.0 });
            // enemyFrontIndex remains the same since we're adding to the back
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


// Temporary for Halloween, 'HIGH : HIGH' instead of 'HIGH : LOW'
    if (showHint) {
      digitalWrite(PIN_HINT_RED, (leadColourNow == 2) ? HIGH : HIGH);
      digitalWrite(PIN_HINT_BLUE, (leadColourNow == 1) ? HIGH : HIGH);
      digitalWrite(PIN_HINT_GREEN, (leadColourNow == 3) ? HIGH : HIGH);
    } else {
      hintLEDsOFF();
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
