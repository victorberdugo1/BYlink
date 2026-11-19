def solve(n, m):
    r = 1 % m
    for i in range(2, n + 1):
        r = r * i % m
    return r
