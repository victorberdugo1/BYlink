def solve(n, m):
    l = 1
    for i in range(2, n + 1):
        a, b = l, i
        while b:
            a, b = b, a % b
        l = l // a * i
    return l % m
