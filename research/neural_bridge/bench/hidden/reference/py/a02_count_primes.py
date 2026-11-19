def solve(n):
    count = 0
    for k in range(2, n):
        d = 2
        while d * d <= k and k % d:
            d += 1
        if d * d > k:
            count += 1
    return count
