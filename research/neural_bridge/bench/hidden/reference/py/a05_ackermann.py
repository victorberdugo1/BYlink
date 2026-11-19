def solve(m, n):
    if m == 0:
        return n + 1
    if n == 0:
        return solve(m - 1, 1)
    return solve(m - 1, solve(m, n - 1))
