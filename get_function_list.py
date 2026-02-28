# get_function_list.py
# Extract all function definitions with line numbers

import re
import sys

# Regex to match a C/Arduino function definition
FUNC_RE = re.compile(
    r'''^
        ([\w:\*\&\s\<\>]+?)      # return type (group 1)
        \s+                      # whitespace
        ([A-Za-z_]\w*)           # function name (group 2)
        \s*\(                    # opening parenthesis
        ([^\)]*)                 # parameters (group 3)
        \)\s*                    # closing parenthesis
        (\{)?                    # optional opening brace (group 4)
    ''',
    re.VERBOSE
)

def extract_functions(path):
    functions = []
    pending = None  # holds signature if "{" is on next line

    with open(path, "r", encoding="utf-8") as f:
        for lineno, line in enumerate(f, start=1):
            stripped = line.strip()

            # If previous line had signature but no "{", check this line
            if pending and stripped.startswith("{"):
                functions.append((pending["lineno"], pending["signature"]))
                pending = None
                continue

            m = FUNC_RE.match(stripped)
            if m:
                return_type, name, params, brace = m.groups()
                signature = f"{return_type.strip()} {name}({params.strip()})"

                if brace:
                    # Full definition on one line
                    functions.append((lineno, signature))
                else:
                    # Opening brace is probably on next line
                    pending = {"lineno": lineno, "signature": signature}

    return functions


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python extract_functions.py <file.ino>")
        sys.exit(1)

    path = sys.argv[1]
    funcs = extract_functions(path)

    for lineno, sig in funcs:
        print(f"{lineno:4d} | {sig}")
