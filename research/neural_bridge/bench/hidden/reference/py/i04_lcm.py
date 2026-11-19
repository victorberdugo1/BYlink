def solve(a, b):
    x, y = a, b
    while y:
        x, y = y, x % y
    return a // x * b
