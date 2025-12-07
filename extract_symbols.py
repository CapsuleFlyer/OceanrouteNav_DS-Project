import re

FILE = "main.cpp"

# Keywords we never want counted as functions
CONTROL_KEYWORDS = {
    "if", "for", "while", "switch", "catch", "else", "do"
}

struct_pattern = re.compile(r'^\s*struct\s+(\w+)')
class_pattern  = re.compile(r'^\s*class\s+(\w+)')

# Function must:
# - start at line beginning
# - NOT be a control statement
# - end with {
function_pattern = re.compile(
    r'^\s*([a-zA-Z_][\w:<>\s*&]+)\s+([a-zA-Z_]\w*)\s*\([^;]*\)\s*\{'
)

with open(FILE, "r", encoding="utf-8", errors="ignore") as f:
    lines = f.readlines()

symbols = []

for line_num, line in enumerate(lines, 1):
    m = struct_pattern.match(line)
    if m:
        symbols.append(("STRUCT", m.group(1), line_num))
        continue

    m = class_pattern.match(line)
    if m:
        symbols.append(("CLASS", m.group(1), line_num))
        continue

    m = function_pattern.match(line)
    if m:
        name = m.group(2)
        if name not in CONTROL_KEYWORDS:
            symbols.append(("FUNCTION", name, line_num))

print("\n===== SYMBOL MAP =====\n")
for kind, name, line in symbols:
    print(f"{kind:9} {name:30} line {line}")
