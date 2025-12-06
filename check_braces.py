# check_braces.py
# Checks for unmatched braces in a C++ file

filename = r"C:\Users\Awan\Documents\GitHub\OceanrouteNav_DS-Project\main.cpp"

stack = []

with open(filename, 'r', encoding='utf-8') as f:
    for lineno, line in enumerate(f, 1):
        for char in line:
            if char == '{':
                stack.append(lineno)
            elif char == '}':
                if stack:
                    stack.pop()
                else:
                    print(f"Extra closing brace '}}' at line {lineno}")

if stack:
    for lineno in stack:
        print(f"Unmatched opening brace '{{' at line {lineno}")
else:
    print("All braces match!")
