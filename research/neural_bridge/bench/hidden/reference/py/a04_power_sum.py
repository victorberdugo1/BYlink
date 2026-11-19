def solve(n, e, m):
    total = 0
    for k in range(1, n + 1):
        total = (total + pow(k, e, m)) % m
    return total
