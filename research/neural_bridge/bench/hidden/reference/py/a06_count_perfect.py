def solve(n):
    count = 0
    for k in range(2, n + 1):
        if sum(d for d in range(1, k) if k % d == 0) == k:
            count += 1
    return count
