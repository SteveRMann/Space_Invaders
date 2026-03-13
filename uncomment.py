#!/usr/bin/env python3
import sys
import shutil
from pathlib import Path

def main():
    if len(sys.argv) != 2:
        print("Usage: uncomment.py <filename>")
        sys.exit(1)

    infile = Path(sys.argv[1])

    if not infile.exists():
        print(f"Error: {infile} does not exist")
        sys.exit(1)

    # Create backup
    backup = infile.with_suffix(infile.suffix + ".bak")
    shutil.copy2(infile, backup)

    cleaned_lines = []
    with infile.open("r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            stripped = line.lstrip()

            # Skip comment lines
            if stripped.startswith("#") or stripped.startswith(";"):
                continue

            # Skip blank or whitespace-only lines
            if stripped.strip() == "":
                continue

            cleaned_lines.append(line)

    # Write cleaned file back
    with infile.open("w", encoding="utf-8") as f:
        f.writelines(cleaned_lines)

    print(f"Backup created: {backup}")
    print(f"Comments and blank lines removed from: {infile}")

if __name__ == "__main__":
    main()
