Game loosely based on Space Invaders where the player "shoots" colored beams at same-color incoming lights.

# Space Invaders – RGB LED Strip Game (ESP32)

A one-player, color-matching "Space Invaders" game on a WS2812B LED strip, driven by an ESP32. A wave of colored invaders crawls along the strip toward your home base. Shoot each one with the button of the matching color before they reach you.

It was built as a Halloween attraction and runs completely stand-alone: there's no Wi-Fi and no web page, and all settings are in the sketch.

---

## How it plays

1. **Waiting:** while idle, random white "invader" streams drift along the strip. Each starts as a single LED, grows one LED at a time, then vanishes and respawns somewhere else. LED 0 and the status LED blink.
2. **Start:** press **any** button. The strip clears, and the game starts when you release the button, so that press never fires a shot.
3. **Get ready:** the start tune plays and the strip stays dark for 2 seconds.
4. **Play:** a wave of red, green and blue invaders moves toward the white home base at the start of the strip.
   - Press the button that matches the color of the **front** invader to destroy it.
   - A **wrong color** adds an invader of that color to the **end** of the wave.
5. **Win:** clear the whole wave and the strip flashes green, then goes back to waiting.
6. **Lose:** if an invader reaches the home base, the base blinks red and white, the strip flashes red, then it goes back to waiting.
7. **Reset:** the white button during a game returns to waiting at any time.

---

## Hardware

| Part | Notes |
|---|---|
| ESP32 DevKit (ESP32-WROOM-32, 4 MB flash) | 30-pin "DevKit V1" style board. A classic ESP32, **not** an S3. |
| WS2812B LED strip | 300 game LEDs plus 1 sacrificial LED (up to 480 supported) |
| 5 push buttons | Blue, Red, Green, White (reset), Continue. They connect to GND; the ESP32's internal pull-ups are used. |
| 3 hint LEDs | Red, Blue, Green, with series resistors |
| 1 status LED | Blinks while waiting, with a series resistor |
| Piezo speaker / buzzer | Plays the game sounds |
| 5 V power supply | Sized for the strip. The sketch limits current to 2.5 A. |
| 330 Ω resistor | In series with the strip's data line, at the ESP32 end |
| 1000 µF capacitor | Across the strip's 5 V and GND, where power enters |

### The sacrificial LED

LED 0 is a "sacrificial" pixel placed close to the ESP32. It takes the ESP32's 3.3 V data signal and passes a clean 5 V signal to the rest of the strip, so it works as a cheap level shifter. The game starts at LED 1. Set `config_sacrifice_led = false` if you use a real level shifter instead.

---

## Wiring

| Function | ESP32 GPIO | Board label |
|---|---|---|
| LED strip data (through 330 Ω) | 15 | D15 |
| Blue button | 25 | D25 |
| Red button | 33 | D33 |
| Green button | 26 | D26 |
| White button (reset / start) | 32 | D32 |
| Continue button (start) | 27 | D27 |
| Hint LED – Red | 14 | D14 |
| Hint LED – Blue | 12 | D12 |
| Hint LED – Green | 13 | D13 |
| Status LED | 19 | D19 |
| Speaker | 4 | D4 |

Each button goes between its GPIO and GND.

### Wiring notes

- **Common ground:** run GND from the ESP32 to the strip's GND, alongside the data wire.
- **Keep the ESP32-to-LED 0 wire short,** ideally under about 30 cm. The sacrificial LED regenerates the signal, so the rest of the strip can be any length.
- **GPIO12 is a boot strapping pin.** If something pulls it HIGH at power-up, the ESP32 won't boot. If you see boot loops, move the blue hint LED to a free pin such as 18 or 21.
- **GPIO15 is also a strapping pin.** It works, but GPIO18 or GPIO23 are cleaner choices for the strip data line.
- **GPIO34, 35, 36 (VP) and 39 (VN) are input-only.** They can't drive the strip. They can be used for buttons only with external 10 kΩ pull-up resistors.

---

## Software setup

1. Install the **Arduino IDE** (2.x).
2. In **Boards Manager**, install **esp32 by Espressif Systems**.
   - Core **2.0.17** is recommended. Core 3.x also compiles the sketch.
3. In **Library Manager**, install **FastLED**.
4. Open `Space_Invaders_V15.ino`.
5. Select these board settings:

| Setting | Value |
|---|---|
| Board | ESP32 Dev Module |
| Flash Size | 4MB (32Mb) |
| Partition Scheme | Default |
| PSRAM | Disabled |
| Upload Speed | 921600 (use 115200 if uploads are flaky) |

6. Upload over USB. If upload stalls at `Connecting....`, hold the **BOOT** button until it starts.

---

## Configuration

Everything is in the **CONFIG** section near the top of the sketch. Edit the values and re-upload; nothing is stored on the ESP32.

### Game

| Setting | Default | Description |
|---|---|---|
| `config_num_leds` | 300 | Game LEDs, not counting the sacrificial LED (30–480) |
| `config_brightness_pct` | 15 | Game brightness, 10–100 |
| `config_sacrifice_led` | true | LED 0 is a sacrificial level-shifter LED |
| `config_homebase_size` | 3 | LEDs in the white home base |
| `config_shot_speed_pct` | 100 | Player shot speed, % |
| `config_enemy_count` | 15 | Invaders in the wave |
| `config_enemy_speed` | 5 | Invader speed, in LEDs per second |
| `config_sound_on` | true | Sound on/off |
| `RESULT_FLASH_MS` | 1500 | How long the green WIN / red LOSE flash lasts, in ms |
| `START_PAUSE_MS` | 2000 | Dark "get ready" pause before the invaders appear, in ms |

### Wait screen

Each MIN/MAX pair is a range; every stream picks a random value between the two numbers.

| Setting | Default | Description |
|---|---|---|
| `config_wait_brightness_pct` | 10 | Wait-screen brightness, 10–100 (same scale as the game brightness) |
| `WAIT_STREAMS` | 3 | Streams on the strip at once |
| `WAIT_SPEED_MIN` / `WAIT_SPEED_MAX` | 4 / 20 | Stream speed, in LEDs per second |
| `WAIT_LIFE_MIN_MS` / `WAIT_LIFE_MAX_MS` | 3000 / 12000 | How long a stream lives |
| `WAIT_DELAY_MIN_MS` / `WAIT_DELAY_MAX_MS` | 500 / 4000 | Delay before a new stream spawns |

Each stream's length is random, from 1 up to `config_enemy_count`.

### Sounds

Each sound is a string of `frequency,duration` pairs separated by semicolons, with frequency in Hz and duration in ms. A frequency of `0` is a rest.

```cpp
const String SND_START = "523,80;659,80;784,80;1047,300";
```

### Hint LEDs

The hint LEDs are meant to light the color of the front invader. For Halloween they're set to stay **on all the time**. To get real hints, change `HIGH : HIGH` to `HIGH : LOW` in the hint section of `updatePlaying()`, and make `hintLEDsOFF()` write `LOW`.

---

## Tools

`LED_Blink_Test.ino` is a small hardware test sketch that uses the same pins as the game. It:

- blinks the onboard LED, the status LED and the hint LEDs together
- cycles the first 20 strip LEDs red → green → blue → off
- prints which buttons are pressed to Serial Monitor at 115200

Use it to check a board or the wiring.

---

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `This chip is ESP32, not ESP32-S3` | The wrong board is selected. Choose **ESP32 Dev Module**. |
| `Failed to connect to ESP32: No serial data received` | Hold **BOOT** while uploading, use a known-good USB data cable, and unplug the project wiring to rule out a strapping-pin problem. |
| No COM port appears | Install the USB driver for your board: CH340 or CP210x, depending on the chip. |
| Boot loops | Something is holding GPIO12 HIGH at power-up. See the wiring notes. |
| Random flickering pixels | Usually a timing problem with ESP32 core 3.x and its newer RMT driver. Use core 2.0.17, keep the data wire short, and check the 330 Ω resistor and common ground. |
| Slow compiles on Windows | Add antivirus exclusions for `%LOCALAPPDATA%\Arduino15`, `%LOCALAPPDATA%\Temp\arduino` and the sketch folder. Turn off verbose compile output, and stop cleanup tools from clearing `Temp\arduino`. |

---

## Version history

| Version | Changes |
|---|---|
| V1 | Stripped the sound and I2S code from the original |
| V2 | Added a Continue button to go to the next level |
| V3 | Added hint LEDs; renamed the White button to Reset |
| V4 | UDP commands to play sounds |
| V5 | Restored the web page for setting Wi-Fi credentials |
| V6 | Debounce for the RGB buttons |
| V7 | Replaced the UDP sounds with `tone()`; missed sprites go to the end of the wave |
| V8 | Fixed an upload problem; added a hint option |
| V9 | Removed game statistics |
| V10 | Removed scoring; Halloween mode (hint LEDs always on); strip goes black after the win/lose flash |
| V11 | Removed all web code (Wi-Fi, web server, web OTA, UDP) and stored settings; all settings now in the sketch |
| V12 | Simple single-level game: removed bosses, the 10-level table, difficulty profiles and the menu |
| V13 | "Press any button to start" |
| V14 | Removed the green level-bar intro and added a dark "get ready" pause |
| V15 | Wait screen with random white invader streams, with its own brightness setting |
