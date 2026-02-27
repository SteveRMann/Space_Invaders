# get arduino settings.py

import os
import json

cache_dir = r"C:\Users\steve\AppData\Local\Arduino15\cache\sketches"

# Find newest build folder
latest = None
latest_time = 0

for root, dirs, files in os.walk(cache_dir):
    for d in dirs:
        path = os.path.join(root, d, "build.options.json")
        if os.path.exists(path):
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

print("=== ESP32-S3 Arduino IDE Settings ===\n")
for k, v in opts.items():
    if "esp32s3" in str(v).lower() or "esp32s3" in k.lower():
        print(f"{k} = {v}")

print("\n=== End ===")
