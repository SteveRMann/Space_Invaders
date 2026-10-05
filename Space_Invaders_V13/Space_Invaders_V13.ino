/*
==========================================================================
PROJECT: Space Invaders - single level

Board Settings
Board: ESP32 Dev Module (ESP32-WROOM-32 DevKit, 4 MB flash)
Flash Size: 4MB
Partition: Default
PSRAM: Disabled

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
V12 Simple single-level game: removed bosses, boss/combo mode, the 10-level
    table, difficulty profiles, the "game finished" rainbow and the menu.
V13 "Press any button to start": the game waits on a black strip (LED 0 and
    GPIO19 blinking) at power-up and after every game; any of the 5 buttons
    starts it. No separate Continue/start button needed.

HOW IT PLAYS
  - Waiting: strip black, LED 0 and GPIO19 blink. Press ANY button to start.
    (The game starts when the button is released, so that press never fires a shot.)
  - Intro: a green bar flashes in the middle of the strip (~4 s), then the
    wave of coloured invaders starts moving toward the white home base.
  - Press the button matching the colour of the front invader to destroy it.
    Wrong colour = that colour is added to the END of the wave.
  - Clear the wave  -> green flash, then black and waiting.
  - Invaders reach the base -> base blinks, red flash, then black and waiting.
  - RESET (white button) during a game returns to waiting.
==========================================================================
*/

#define VERSION 13.0

#include "esp32-hal-ledc.h"  // For tone function
#include <vector>
#include <FastLED.h>

// ============================================================================
// CONFIG  - edit these and re-upload
// ============================================================================
int config_num_leds = 300;         // Game LEDs (30..MAX_LEDS), not counting the sacrificial LED
int config_brightness_pct = 15;    // 10..100
bool config_sacrifice_led = true;  // true = LED 0 is a level-shifter "sacrificial" LED
int config_homebase_size = 3;      // LEDs in the home base
int config_shot_speed_pct = 100;   // Player shot speed, %
int config_enemy_count = 15;       // Invaders in the wave
int config_enemy_speed = 5;        // Invader speed (LEDs per second)

// AUDIO CONFIG
bool config_sound_on = true;
int config_volume_pct = 50;

#define RESULT_FLASH_MS 1000  // How long the green WIN / red LOSE strip stays lit before going black
#define INTRO_MS 4000         // Length of the intro (bar steady 2 s, then flashing 2 s)
#define FRAME_DELAY 16        // 16ms = approx. 60 FPS
const int FIRE_COOLDOWN = 100;

// ============================================================================
// PINS
// ============================================================================
#define PIN_LED_DATA 15
#define MAX_LEDS 480
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

#define PIN_BTN_BLUE 25
#define PIN_BTN_RED 33
#define PIN_BTN_GREEN 26
#define PIN_BTN_WHITE 32     // Reset (during a game) / start
#define PIN_BTN_CONTINUE 27  // Start (any button starts the game)

// Colour-hint GPIOs (external LEDs for the player)
#define PIN_HINT_RED 14
#define PIN_HINT_BLUE 12
#define PIN_HINT_GREEN 13

// Status LED - blinks with the sacrificial LED while waiting for a button press
#define PIN_GPIO19_LED 19

#define SPEAKER_PIN 4

// ============================================================================
// SOUNDS  ("freq,ms;freq,ms;...")
// ============================================================================
const String SND_START = "523,80;659,80;784,80;1047,300";
const String SND_WIN = "523,80;659,80;784,80;1047,300;0,150;1047,60;1319,60";
const String SND_LOSE = "370,100;349,100;330,100;311,400";
const String SND_MISTAKE = "60,150";
const String SND_SHOT_BLUE = "698,50;659,50";
const String SND_SHOT_RED = "784,30;1047,30;1319,30";
const String SND_SHOT_GREEN = "523,30;554,30;523,30";
const String SND_HIT = "2093,30";

// ============================================================================
// COLOURS  (1 = blue, 2 = red, 3 = green)
// ============================================================================
const CRGB COL_BLUE = CRGB(0x00, 0x00, 0xFF);
const CRGB COL_RED = CRGB(0xFF, 0x00, 0x00);
const CRGB COL_GREEN = CRGB(0x00, 0xFF, 0x00);
const CRGB COL_SACRIFICE = CRGB(20, 0, 0);

// ============================================================================
// GAME STATE
// ============================================================================
enum GameState {
  STATE_READY,           // black, LED 0 + GPIO19 blinking, waiting for any button
  STATE_INTRO,
  STATE_PLAYING,
  STATE_WON,             // green flash, then -> STATE_READY
  STATE_BASE_DESTROYED,  // base blinks red/white for 2 s
  STATE_GAMEOVER         // red flash, then -> STATE_READY
};

struct Shot {
  float position;
  int color;
};

CRGB leds[MAX_LEDS + 1];  // +1 for the sacrificial LED
int ledStartOffset = 1;   // Set automatically from config_sacrifice_led

GameState currentState = STATE_READY;
std::vector<int> enemies;  // colour of each invader, front first
std::vector<Shot> shots;
float enemyFrontIndex = 0;

unsigned long lastLoopTime = 0;
unsigned long stateTimer = 0;
unsigned long lastFireTime = 0;
bool buttonsReleased = true;
bool btnWhiteHeld = false;

// "Press any button" start: armed once all buttons are released,
// start fires when the pressed button is let go.
bool readyArmed = false;
bool startPending = false;

// Waiting blink (LED 0 + GPIO19)
unsigned long lastBlink = 0;
bool gpio19State = false;

// Colour-hint state
int lastLeadColour = -1;
bool hintPending = false;
bool hitJustOccurred = false;


// ============================================================================
// HELPERS
// ============================================================================
CRGB getColor(int colorCode) {
  switch (colorCode) {
    case 1: return COL_BLUE;
    case 2: return COL_RED;
    case 3: return COL_GREEN;
    default: return CRGB::Black;
  }
}

void drawCrispPixel(float pos, CRGB color) {
  int idx = round(pos);
  if (idx < 0 || idx >= config_num_leds) return;
  leds[idx + ledStartOffset] = color;
}

void flashPixel(int pos) {
  if (pos >= 0 && pos < config_num_leds) leds[pos + ledStartOffset] = CRGB::White;
}

void fillGame(CRGB c) {
  fill_solid(&leds[ledStartOffset], config_num_leds, c);
}

void showFrame() {
  if (config_sacrifice_led) leds[0] = COL_SACRIFICE;
  FastLED.show();
}

// Hint LEDs: Halloween setting - all three stay ON.
void hintLEDsOFF() {
  digitalWrite(PIN_HINT_RED, HIGH);
  digitalWrite(PIN_HINT_BLUE, HIGH);
  digitalWrite(PIN_HINT_GREEN, HIGH);
}

void hintLEDsON() {
  digitalWrite(PIN_HINT_RED, HIGH);
  digitalWrite(PIN_HINT_BLUE, HIGH);
  digitalWrite(PIN_HINT_GREEN, HIGH);
}

void setStatusLed(bool on) {
  gpio19State = on;
  digitalWrite(PIN_GPIO19_LED, on ? HIGH : LOW);
}

bool anyButtonDown() {
  return digitalRead(PIN_BTN_BLUE) == LOW || digitalRead(PIN_BTN_RED) == LOW || digitalRead(PIN_BTN_GREEN) == LOW || digitalRead(PIN_BTN_WHITE) == LOW || digitalRead(PIN_BTN_CONTINUE) == LOW;
}


// ============================================================================
// AUDIO
// ============================================================================
void playToneLocal(int freq, int duration_ms) {
  if (!config_sound_on || config_volume_pct == 0) return;
  tone(SPEAKER_PIN, freq, duration_ms);
}

void playSequence(const String &seq) {
  int start = 0;
  while (start < (int)seq.length()) {
    int end = seq.indexOf(';', start);
    if (end == -1) end = seq.length();
    String pair = seq.substring(start, end);
    int comma = pair.indexOf(',');
    if (comma != -1) {
      int freq = pair.substring(0, comma).toInt();
      int dur = pair.substring(comma + 1).toInt();
      if (freq > 0 && dur > 0) {
        playToneLocal(freq, dur);
        delay(5);  // Small gap between tones
      }
    }
    start = end + 1;
  }
}

void playShotSound(int color) {
  if (color == 1) playSequence(SND_SHOT_BLUE);
  else if (color == 2) playSequence(SND_SHOT_RED);
  else if (color == 3) playSequence(SND_SHOT_GREEN);
}


// ============================================================================
// GAME FLOW
// ============================================================================

/* Draws the intro screen: dim strip with a green bar in the middle. */
void drawIntro(uint8_t dim) {
  fillGame(CRGB(dim, dim, dim));
  int start = config_num_leds / 2 - 3;
  for (int k = 0; k < 6; k++) leds[start + k + ledStartOffset] = COL_GREEN;
  showFrame();
}

/* Starts a new game from scratch (called from the waiting state). */
void startGame() {
  enemies.clear();
  shots.clear();
  buttonsReleased = true;
  lastFireTime = 0;
  lastLeadColour = -1;
  hintPending = false;
  hitJustOccurred = false;
  setStatusLed(false);

  playSequence(SND_START);
  currentState = STATE_INTRO;
  stateTimer = millis();
  drawIntro(10);
  hintLEDsON();  // Halloween: hint LEDs always on
}

/* Intro: bar steady for 2 s, flashing for 2 s, then the wave starts. */
void updateIntro() {
  unsigned long elapsed = millis() - stateTimer;
  if (elapsed < INTRO_MS / 2) {
    drawIntro(5);
  } else if (elapsed < INTRO_MS) {
    if ((elapsed / 250) % 2 == 0) drawIntro(5);
    else {
      fillGame(CRGB::Black);
      showFrame();
    }
  } else {
    int count = config_enemy_count > 0 ? config_enemy_count : 10;
    for (int i = 0; i < count; i++) enemies.push_back((int)random(1, 4));
    enemyFrontIndex = (float)config_num_leds - 1.0;
    currentState = STATE_PLAYING;
  }
}

void winGame() {
  playSequence(SND_WIN);
  currentState = STATE_WON;
  stateTimer = millis();
}

void triggerBaseDestruction() {
  playSequence(SND_LOSE);
  currentState = STATE_BASE_DESTROYED;
  stateTimer = millis();
  hintLEDsOFF();
}

/* Waiting state: used at power-up and after every game. */
void enterReady() {
  currentState = STATE_READY;
  readyArmed = false;    // buttons must be released first (no accidental restart)
  startPending = false;
}

/* Strip black, LED 0 and GPIO19 blink at 2 Hz.
   Any button starts the game - on release, so the press never fires a shot. */
void updateReady() {
  unsigned long now = millis();
  if (anyButtonDown()) {
    if (readyArmed) startPending = true;
  } else {
    if (startPending) {
      startGame();
      return;
    }
    readyArmed = true;
  }

  fillGame(CRGB::Black);
  if (now - lastBlink >= 250) {
    lastBlink = now;
    setStatusLed(!gpio19State);
  }
  if (config_sacrifice_led) leds[0] = gpio19State ? COL_SACRIFICE : CRGB::Black;
  FastLED.show();
}

/* Win screen: green flash for RESULT_FLASH_MS, then black and waiting. */
void updateWon() {
  if (millis() - stateTimer < RESULT_FLASH_MS) {
    fillGame(COL_GREEN);
    showFrame();
  } else {
    enterReady();
  }
}

/* Home base blinks red/white for 2 s while the rest of the strip fades. */
void updateBaseDestroyed() {
  unsigned long elapsed = millis() - stateTimer;
  if (elapsed < 2000) {
    CRGB c = (elapsed / 100) % 2 == 0 ? COL_RED : CRGB::White;
    for (int i = 0; i < config_homebase_size; i++) leds[i + ledStartOffset] = c;
    for (int i = config_homebase_size; i < config_num_leds; i++) leds[i + ledStartOffset].nscale8(240);
    showFrame();
  } else {
    currentState = STATE_GAMEOVER;
    stateTimer = millis();
  }
}

/* Lose screen: red flash for RESULT_FLASH_MS, then black and waiting. */
void updateGameOver() {
  if (millis() - stateTimer < RESULT_FLASH_MS) {
    fillGame(COL_RED);
    showFrame();
  } else {
    enterReady();
  }
}


/* One frame of actual gameplay. */
void updatePlaying(unsigned long now) {
  // ---------------- INPUT ----------------
  bool b = (digitalRead(PIN_BTN_BLUE) == LOW);
  bool r = (digitalRead(PIN_BTN_RED) == LOW);
  bool g = (digitalRead(PIN_BTN_GREEN) == LOW);
  bool anyPressed = (b || r || g);

  if (!anyPressed) buttonsReleased = true;
  else hintPending = false;

  if (anyPressed && buttonsReleased && (now - lastFireTime > FIRE_COOLDOWN)) {
    int c = b ? 1 : (r ? 2 : 3);
    shots.push_back({ 0.0, c });
    lastFireTime = now;
    playShotSound(c);
    buttonsReleased = false;
  }

  // ---------------- SHOTS ----------------
  float moveStep = (float)config_shot_speed_pct / 60.0 * 0.6;
  if (moveStep < 0.2) moveStep = 0.2;

  for (int i = shots.size() - 1; i >= 0; i--) {
    shots[i].position += moveStep;
    bool remove = false;

    if (shots[i].position >= enemyFrontIndex && !enemies.empty()) {
      if (shots[i].color == enemies.front()) {  // correct hit
        enemies.erase(enemies.begin());
        enemyFrontIndex += 1.0;
        flashPixel((int)shots[i].position);
        hitJustOccurred = true;
        playSequence(SND_HIT);
      } else {  // wrong colour: add the player's colour to the END of the wave
        enemies.push_back(shots[i].color);
        playSequence(SND_MISTAKE);
      }
      remove = true;
    }
    if (shots[i].position >= config_num_leds) remove = true;
    if (remove) shots.erase(shots.begin() + i);
  }

  if (enemies.empty()) {
    winGame();
    return;
  }

  // ---------------- INVADERS ----------------
  enemyFrontIndex -= (float)config_enemy_speed / 60.0;
  if (enemyFrontIndex <= config_homebase_size) {
    triggerBaseDestruction();
    return;
  }

  // ---------------- DRAW ----------------
  fillGame(CRGB::Black);
  for (size_t i = 0; i < enemies.size(); i++)
    drawCrispPixel(enemyFrontIndex + (float)i, getColor(enemies[i]));
  for (auto &s : shots)
    drawCrispPixel(s.position, getColor(s.color));
  for (int i = 0; i < config_homebase_size; i++)
    leds[i + ledStartOffset] = CRGB::White;

  // ---------------- HINT LEDs ----------------
  int leadColourNow = enemies.front();
  bool showHint = false;
  if (!anyPressed) {
    if (leadColourNow != lastLeadColour || hitJustOccurred) showHint = true;
    else showHint = hintPending;
  }
  // Halloween: all hint LEDs stay ON. For real hints use (leadColourNow == x) ? HIGH : LOW
  if (showHint) {
    digitalWrite(PIN_HINT_RED, (leadColourNow == 2) ? HIGH : HIGH);
    digitalWrite(PIN_HINT_BLUE, (leadColourNow == 1) ? HIGH : HIGH);
    digitalWrite(PIN_HINT_GREEN, (leadColourNow == 3) ? HIGH : HIGH);
  } else {
    hintLEDsOFF();
  }
  hintPending = showHint;
  lastLeadColour = leadColourNow;
  hitJustOccurred = false;

  showFrame();
}


// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial.printf("\n\n=== SPACE INVADERS V%.1f ===\n", VERSION);

  pinMode(PIN_BTN_BLUE, INPUT_PULLUP);
  pinMode(PIN_BTN_RED, INPUT_PULLUP);
  pinMode(PIN_BTN_GREEN, INPUT_PULLUP);
  pinMode(PIN_BTN_WHITE, INPUT_PULLUP);
  pinMode(PIN_BTN_CONTINUE, INPUT_PULLUP);

  pinMode(PIN_HINT_RED, OUTPUT);
  pinMode(PIN_HINT_BLUE, OUTPUT);
  pinMode(PIN_HINT_GREEN, OUTPUT);
  hintLEDsOFF();

  pinMode(PIN_GPIO19_LED, OUTPUT);
  setStatusLed(false);

  pinMode(SPEAKER_PIN, OUTPUT);
  digitalWrite(SPEAKER_PIN, LOW);

  // --- CONFIG ---
  ledStartOffset = config_sacrifice_led ? 1 : 0;
  if (config_num_leds < 30) config_num_leds = 30;
  if (config_num_leds > MAX_LEDS) config_num_leds = MAX_LEDS;
  Serial.printf("LEDs: %d (+%d sacrificial), invaders: %d, speed: %d\n",
                config_num_leds, ledStartOffset, config_enemy_count, config_enemy_speed);

  randomSeed(esp_random());

  // --- FASTLED ---
  FastLED.addLeds<LED_TYPE, PIN_LED_DATA, COLOR_ORDER>(leds, config_num_leds + ledStartOffset);
  FastLED.setBrightness(map(config_brightness_pct, 10, 100, 25, 255));
  FastLED.setDither(0);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2500);

  // Power-up test pattern: every other LED green for 0.5 s
  FastLED.clear();
  for (int i = 0; i < config_num_leds; i += 2) leds[i + ledStartOffset] = CRGB::Green;
  showFrame();
  delay(500);

  enterReady();  // wait for any button
}


// ============================================================================
// LOOP
// ============================================================================
void loop() {
  unsigned long now = millis();
  if (now - lastLoopTime < FRAME_DELAY) return;
  lastLoopTime = now;

  // RESET (white) during a game - back to waiting on release
  if (currentState != STATE_READY) {
    if (digitalRead(PIN_BTN_WHITE) == LOW) {
      btnWhiteHeld = true;
    } else if (btnWhiteHeld) {
      btnWhiteHeld = false;
      enterReady();
      return;
    }
  } else {
    btnWhiteHeld = false;
  }

  switch (currentState) {
    case STATE_READY: updateReady(); break;
    case STATE_INTRO: updateIntro(); break;
    case STATE_PLAYING: updatePlaying(now); break;
    case STATE_WON: updateWon(); break;
    case STATE_BASE_DESTROYED: updateBaseDestroyed(); break;
    case STATE_GAMEOVER: updateGameOver(); break;
  }
}
