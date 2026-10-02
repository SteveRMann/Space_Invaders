/*
  LED Blink Test for the Space Invaders hardware (ESP32 Dev Module)
  - Onboard blue LED (GPIO2), GPIO19 status LED, hint LEDs (GPIO12/13/14)
    all toggle once per second.
  - WS2812B strip on GPIO15 cycles red -> green -> blue -> off on the
    first 20 LEDs (low brightness, safe on USB power).
  - Serial (115200) prints a heartbeat and which game buttons are pressed.
*/

#include <FastLED.h>

#define PIN_LED_DATA   15
#define TEST_LEDS      21      // 1 sacrificial + 20 game LEDs
#define PIN_ONBOARD    2
#define PIN_STATUS     19
#define PIN_HINT_RED   14
#define PIN_HINT_BLUE  12
#define PIN_HINT_GREEN 13

const int buttons[] = { 25, 33, 26, 32, 27 };
const char *btnNames[] = { "BLUE", "RED", "GREEN", "WHITE/RESET", "CONTINUE" };

CRGB leds[TEST_LEDS];

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== LED Blink Test ===");

  pinMode(PIN_ONBOARD, OUTPUT);
  pinMode(PIN_STATUS, OUTPUT);
  pinMode(PIN_HINT_RED, OUTPUT);
  pinMode(PIN_HINT_BLUE, OUTPUT);
  pinMode(PIN_HINT_GREEN, OUTPUT);
  for (int b : buttons) pinMode(b, INPUT_PULLUP);

  FastLED.addLeds<WS2812B, PIN_LED_DATA, GRB>(leds, TEST_LEDS);
  FastLED.setBrightness(40);
}

void loop() {
  static uint8_t step = 0;
  bool on = (step % 2 == 0);

  digitalWrite(PIN_ONBOARD, on);
  digitalWrite(PIN_STATUS, on);
  digitalWrite(PIN_HINT_RED, on);
  digitalWrite(PIN_HINT_BLUE, on);
  digitalWrite(PIN_HINT_GREEN, on);

  const CRGB colors[] = { CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Black };
  const char *colorNames[] = { "RED", "GREEN", "BLUE", "OFF" };
  fill_solid(leds, TEST_LEDS, colors[step % 4]);
  FastLED.show();

  Serial.printf("tick %u  GPIO LEDs %s  strip %s  buttons:",
                step, on ? "ON " : "OFF", colorNames[step % 4]);
  for (int i = 0; i < 5; i++)
    if (digitalRead(buttons[i]) == LOW) Serial.printf(" %s", btnNames[i]);
  Serial.println();

  step++;
  delay(1000);
}
