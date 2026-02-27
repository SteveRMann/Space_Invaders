"""
get_arduino_settings_wroom32.py
--------------------------------

Extracts the actual ESP32-WROOM-32 board settings from Arduino IDE 2.x
by parsing the FQBN string and reading the real partition CSV filename.

INSTRUCTIONS
------------
1. Open Arduino IDE 2.x
2. Select: Tools → Board → ESP32 Arduino → ESP32 Dev Module
3. Click Verify (compile)
4. Run this script

It will:
- Locate the newest build.options.json (IDE 2.x has two possible locations)
- Parse the FQBN into readable settings
- Detect the actual partition CSV used (default.csv vs default_ffat.csv)
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

# -------------------------------
# Extract FQBN settings
# -------------------------------
fqbn = opts.get("fqbn", "")

if not fqbn:
    print("No FQBN found in build.options.json")
    exit()

print("=== ESP32-WROOM-32 Arduino IDE Settings ===\n")

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

# -------------------------------
# Detect actual partition CSV file
# -------------------------------
partition = opts.get("build.partition", None)

print("\nPartition CSV used by Arduino IDE:")

if partition:
    print(f"  {partition}.csv")
else:
    print("  (Not found in build.options.json)")

print("\n=== End ===")
