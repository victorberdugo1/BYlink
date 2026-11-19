def solve(n):
    count = 0
    for k in range(1, n + 1):
        r, x = 0, k
        while x:
            r = r * 10 + x % 10
            x //= 10
        count += r == k
    return count
