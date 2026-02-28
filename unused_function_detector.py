# unused_function_detector.py
# Extract functions + detect unused ones
# Example use:
# python unused_function_detector.py Space_Invaders.ino > unused_functions.txt

import re
import sys

# Regex to match a C/Arduino function definition
FUNC_RE = re.compile(
    r'''^
        ([\w:\*\&\s\<\>]+?)      # return type
        \s+
        ([A-Za-z_]\w*)           # function name
        \s*\(
        ([^\)]*)                 # parameters
        \)\s*
        (\{)?                    # optional opening brace
    ''',
    re.VERBOSE
)

def strip_comments(text):
    # Remove block comments first
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    # Remove single-line comments
    text = re.sub(r'//.*', '', text)
    return text

def extract_functions(lines):
    functions = []
    pending = None

    for lineno, line in enumerate(lines, start=1):
        stripped = line.strip()

        # If previous line had signature but no "{", check this line
        if pending and stripped.startswith("{"):
            functions.append((pending["lineno"], pending["name"], pending["signature"]))
            pending = None
            continue

        m = FUNC_RE.match(stripped)
        if m:
            return_type, name, params, brace = m.groups()
            signature = f"{return_type.strip()} {name}({params.strip()})"

            if brace:
                functions.append((lineno, name, signature))
            else:
                pending = {"lineno": lineno, "name": name, "signature": signature}

    return functions

def find_calls(clean_text, function_names):
    calls = {name: 0 for name in function_names}

    for name in function_names:
        # Count occurrences of name( but avoid matching definitions
        pattern = rf'\b{name}\s*\('
        matches = re.findall(pattern, clean_text)
        calls[name] = len(matches)

    return calls

def main(path):
    with open(path, "r", encoding="utf-8") as f:
        raw_lines = f.readlines()

    raw_text = "".join(raw_lines)
    clean_text = strip_comments(raw_text)

    functions = extract_functions(raw_lines)
    function_names = [name for (_, name, _) in functions]

    calls = find_calls(clean_text, function_names)

    # Whitelist Arduino entry points
    for entry in ("setup", "loop"):
        if entry in calls:
            calls[entry] = 2

    print("Function definitions:")
    for lineno, name, sig in functions:
        print(f"{lineno:4d} | {sig}")

    print("\nUnused functions:")
    unused = []
    for lineno, name, sig in functions:
        # A function is unused if it appears only once (its own definition)
        if calls[name] <= 1:
            unused.append((lineno, name, sig))
            print(f"{lineno:4d} | {sig}")

    if not unused:
        print("None — all functions are referenced somewhere.")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python find_unused_functions.py <file.ino>")
        sys.exit(1)

    main(sys.argv[1])
