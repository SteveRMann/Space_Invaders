"""
get_arduino_v2_settings.py
--------------------------------

Extracts the actual ESP32-WROOM-32 board settings from Arduino IDE 2.x
by parsing the FQBN string inside build.options.json.

INSTRUCTIONS
------------
1. Open Arduino IDE 2.x
2. Select: Tools → Board → ESP32 Arduino → ESP32 Dev Module
3. Click Verify (compile)
4. Run this script
"""

import os
import json

# Arduino IDE 2.x locations
paths_to_search = [
    r"C:\Users\steve\AppData\Local\arduino\sketches",
    r"C:\Users\steve\AppData\Local\Temp"
]

latest = None
latest_time = 0

# Search for newest build.options.json
for base in paths_to_search:
    for root, dirs, files in os.walk(base):
        if "build.options.json" in files:
            path = os.path.join(root, "build.options.json")
            t = os.path.getmtime(path)
            if t > latest_time:
                latest_time = t
                latest = path

if not latest:
    print("No build.options.json found. Compile a sketch first.")
    exit()

print(f"Using: {latest}\n")

with open(latest, "r") as f:
    opts = json.load(f)

fqbn = opts.get("fqbn", "")

if not fqbn:
    print("No FQBN found in build.options.json")
    exit()

print("=== ESP32-WROOM-32 Arduino IDE Settings ===\n")

# FQBN format:
# esp32:esp32:esp32:UploadSpeed=921600,CPUFreq=240,FlashFreq=80,FlashMode=qio,...

parts = fqbn.split(":")
if len(parts) < 4:
    print("Unexpected FQBN format:", fqbn)
    exit()

settings_str = parts[3]
settings = settings_str.split(",")

for s in settings:
    if "=" in s:
        k, v = s.split("=", 1)
        print(f"{k} = {v}")

print("\n=== End ===")
