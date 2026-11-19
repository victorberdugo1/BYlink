import calendar
import math

SPLITS = {
    "train": ["arith", "cond", "loops", "digits"],
    "val": ["recursion", "divisors", "numeric"],
    "test": ["gcd", "primes", "modular", "collatz", "combinatorics"],
}


def _sieve(limit):
    flags = bytearray([1]) * (limit + 1)
    flags[0:2] = b"\x00\x00"
    for i in range(2, math.isqrt(limit) + 1):
        if flags[i]:
            flags[i * i :: i] = bytearray(len(flags[i * i :: i]))
    return flags


_PRIMES = _sieve(1_000_000)


def _fib(n):
    a, b = 0, 1
    for _ in range(n):
        a, b = b, a + b
    return a


def _collatz(n):
    steps = 0
    while n != 1:
        n = n // 2 if n % 2 == 0 else 3 * n + 1
        steps += 1
    return steps


def _ackermann(m, n):
    if m == 0:
        return n + 1
    if m == 1:
        return n + 2
    if m == 2:
        return 2 * n + 3
    return 2 ** (n + 3) - 3


def _perfect_count(n):
    return sum(1 for p in (6, 28, 496, 8128, 33550336) if p <= n)


def _task(tid, tier, family, spec, params, ret, ref, tests, by, py):
    return {
        "id": tid,
        "tier": tier,
        "family": family,
        "spec": spec,
        "params": params,
        "ret": ret,
        "ref": ref,
        "tests": tests,
        "by": by.strip("\n"),
        "py": py.strip("\n"),
    }


I32 = "i32"
I64 = "i64"

TASKS = [
    _task(
        "b01_sum3", "basic", "arith",
        "Return the sum of the three arguments.",
        [("a", I32), ("b", I32), ("c", I32)], I32,
        lambda a, b, c: a + b + c,
        [(1, 2, 3), (0, 0, 0), (-5, 10, 2), (1000, 2000, 3000), (-7, -8, -9), (2147483000, -1000, 500)],
        """
fn solve(a: i32, b: i32, c: i32) -> i32 {
    return a + b + c;
}
""",
        """
def solve(a, b, c):
    return a + b + c
""",
    ),
    _task(
        "b02_abs_diff", "basic", "arith",
        "Return the absolute difference |a - b|. The result always fits in i32.",
        [("a", I32), ("b", I32)], I32,
        lambda a, b: abs(a - b),
        [(5, 3), (3, 5), (0, 0), (-4, 6), (6, -4), (1000000, -1000000)],
        """
fn solve(a: i32, b: i32) -> i32 {
    if a > b {
        return a - b;
    }
    return b - a;
}
""",
        """
def solve(a, b):
    return a - b if a > b else b - a
""",
    ),
    _task(
        "b03_max3", "basic", "cond",
        "Return the largest of the three arguments.",
        [("a", I32), ("b", I32), ("c", I32)], I32,
        lambda a, b, c: max(a, b, c),
        [(1, 2, 3), (3, 2, 1), (2, 3, 1), (-1, -2, -3), (5, 5, 5), (0, -7, -7)],
        """
fn solve(a: i32, b: i32, c: i32) -> i32 {
    let mut m: i32 = a;
    if b > m {
        m = b;
    }
    if c > m {
        m = c;
    }
    return m;
}
""",
        """
def solve(a, b, c):
    return max(a, b, c)
""",
    ),
    _task(
        "b04_clamp", "basic", "cond",
        "Clamp x into the closed range [lo, hi]. It is guaranteed that lo <= hi.",
        [("x", I32), ("lo", I32), ("hi", I32)], I32,
        lambda x, lo, hi: min(max(x, lo), hi),
        [(5, 0, 10), (-5, 0, 10), (15, 0, 10), (0, 0, 10), (10, 0, 10), (7, 7, 7)],
        """
fn solve(x: i32, lo: i32, hi: i32) -> i32 {
    if x < lo {
        return lo;
    }
    if x > hi {
        return hi;
    }
    return x;
}
""",
        """
def solve(x, lo, hi):
    return lo if x < lo else hi if x > hi else x
""",
    ),
    _task(
        "b05_sign", "basic", "cond",
        "Return -1 if x is negative, 0 if x is zero, and 1 if x is positive.",
        [("x", I32)], I32,
        lambda x: (x > 0) - (x < 0),
        [(5,), (-5,), (0,), (1,), (-1,), (2000000000,)],
        """
fn solve(x: i32) -> i32 {
    if x < 0 {
        return -1;
    }
    if x > 0 {
        return 1;
    }
    return 0;
}
""",
        """
def solve(x):
    return -1 if x < 0 else 1 if x > 0 else 0
""",
    ),
    _task(
        "b06_addmod", "basic", "modular",
        "Return (a + b) mod m. Guaranteed 0 <= a < m, 0 <= b < m and m <= 1000000000.",
        [("a", I32), ("b", I32), ("m", I32)], I32,
        lambda a, b, m: (a + b) % m,
        [(3, 4, 5), (0, 0, 1), (999999999, 999999999, 1000000000), (10, 20, 31), (0, 5, 7), (6, 6, 7)],
        """
fn solve(a: i32, b: i32, m: i32) -> i32 {
    return (a + b) % m;
}
""",
        """
def solve(a, b, m):
    return (a + b) % m
""",
    ),
    _task(
        "b07_mul_add", "basic", "arith",
        "Return a * b + c. Guaranteed that no intermediate value overflows i32.",
        [("a", I32), ("b", I32), ("c", I32)], I32,
        lambda a, b, c: a * b + c,
        [(2, 3, 4), (0, 5, 6), (-3, 4, 5), (1000, 1000, 1), (46340, 46340, 0), (-7, -7, -7)],
        """
fn solve(a: i32, b: i32, c: i32) -> i32 {
    return a * b + c;
}
""",
        """
def solve(a, b, c):
    return a * b + c
""",
    ),
    _task(
        "b08_leap_year", "basic", "cond",
        "Return 1 if year y is a leap year in the Gregorian calendar, otherwise 0. Guaranteed y >= 1.",
        [("y", I32)], I32,
        lambda y: 1 if calendar.isleap(y) else 0,
        [(2000,), (1900,), (2024,), (2023,), (1600,), (2100,)],
        """
fn solve(y: i32) -> i32 {
    if y % 400 == 0 {
        return 1;
    }
    if y % 100 == 0 {
        return 0;
    }
    if y % 4 == 0 {
        return 1;
    }
    return 0;
}
""",
        """
def solve(y):
    return 1 if (y % 4 == 0 and y % 100 != 0) or y % 400 == 0 else 0
""",
    ),
    _task(
        "b09_triangular", "basic", "arith",
        "Return the sum 1 + 2 + ... + n. Guaranteed 0 <= n <= 1000.",
        [("n", I32)], I32,
        lambda n: sum(range(n + 1)),
        [(0,), (1,), (5,), (10,), (100,), (1000,)],
        """
fn solve(n: i32) -> i32 {
    return n * (n + 1) / 2;
}
""",
        """
def solve(n):
    return n * (n + 1) // 2
""",
    ),
    _task(
        "b10_divisible", "basic", "primes",
        "Return 1 if b is not zero and a is divisible by b, otherwise 0.",
        [("a", I32), ("b", I32)], I32,
        lambda a, b: 1 if b != 0 and a % b == 0 else 0,
        [(10, 5), (10, 3), (0, 7), (7, 0), (-9, 3), (2147483647, 1)],
        """
fn solve(a: i32, b: i32) -> i32 {
    if b == 0 {
        return 0;
    }
    if a % b == 0 {
        return 1;
    }
    return 0;
}
""",
        """
def solve(a, b):
    return 1 if b != 0 and a % b == 0 else 0
""",
    ),
    _task(
        "i01_factorial_mod", "intermediate", "loops",
        "Return n! mod m. Guaranteed 0 <= n <= 20 and 1 <= m <= 1000000007. 0! = 1.",
        [("n", I64), ("m", I64)], I64,
        lambda n, m: math.factorial(n) % m,
        [(0, 7), (5, 1000), (10, 1000000007), (20, 998244353), (1, 1), (15, 97)],
        """
fn solve(n: i64, m: i64) -> i64 {
    let mut r: i64 = 1 % m;
    let mut i: i64 = 2;
    while i <= n {
        r = r * i % m;
        i = i + 1;
    }
    return r;
}
""",
        """
def solve(n, m):
    r = 1 % m
    for i in range(2, n + 1):
        r = r * i % m
    return r
""",
    ),
    _task(
        "i02_fib", "intermediate", "recursion",
        "Return the n-th Fibonacci number with F(0) = 0 and F(1) = 1. Guaranteed 0 <= n <= 25.",
        [("n", I32)], I32,
        _fib,
        [(0,), (1,), (2,), (10,), (20,), (25,)],
        """
fn solve(n: i32) -> i32 {
    if n < 2 {
        return n;
    }
    return solve(n - 1) + solve(n - 2);
}
""",
        """
def solve(n):
    return n if n < 2 else solve(n - 1) + solve(n - 2)
""",
    ),
    _task(
        "i03_gcd", "intermediate", "gcd",
        "Return the greatest common divisor of a and b. Guaranteed a >= 0 and b >= 0. gcd(0, 0) = 0.",
        [("a", I32), ("b", I32)], I32,
        math.gcd,
        [(12, 18), (17, 5), (0, 9), (9, 0), (0, 0), (1071, 462)],
        """
fn solve(a: i32, b: i32) -> i32 {
    if b == 0 {
        return a;
    }
    return solve(b, a % b);
}
""",
        """
def solve(a, b):
    while b:
        a, b = b, a % b
    return a
""",
    ),
    _task(
        "i04_lcm", "intermediate", "gcd",
        "Return the least common multiple of a and b. Guaranteed a >= 1, b >= 1 and that the result fits in i32.",
        [("a", I32), ("b", I32)], I32,
        math.lcm,
        [(4, 6), (7, 13), (1, 1), (21, 6), (100, 75), (30, 42)],
        """
fn gcd(a: i32, b: i32) -> i32 {
    if b == 0 {
        return a;
    }
    return gcd(b, a % b);
}

fn solve(a: i32, b: i32) -> i32 {
    return a / gcd(a, b) * b;
}
""",
        """
def solve(a, b):
    x, y = a, b
    while y:
        x, y = y, x % y
    return a // x * b
""",
    ),
    _task(
        "i05_digit_sum", "intermediate", "digits",
        "Return the sum of the decimal digits of n. Guaranteed n >= 0.",
        [("n", I32)], I32,
        lambda n: sum(map(int, str(n))),
        [(0,), (7,), (123,), (9999,), (1000000000,), (987654321,)],
        """
fn solve(n: i32) -> i32 {
    let mut s: i32 = 0;
    let mut x: i32 = n;
    while x > 0 {
        s = s + x % 10;
        x = x / 10;
    }
    return s;
}
""",
        """
def solve(n):
    return sum(int(c) for c in str(n))
""",
    ),
    _task(
        "i06_reverse_digits", "intermediate", "digits",
        "Return n with its decimal digits reversed (trailing zeros of n vanish). Guaranteed 0 <= n <= 999999999.",
        [("n", I32)], I32,
        lambda n: int(str(n)[::-1]),
        [(0,), (5,), (120,), (1234,), (100000,), (987654321,)],
        """
fn solve(n: i32) -> i32 {
    let mut r: i32 = 0;
    let mut x: i32 = n;
    while x > 0 {
        r = r * 10 + x % 10;
        x = x / 10;
    }
    return r;
}
""",
        """
def solve(n):
    r = 0
    while n > 0:
        r = r * 10 + n % 10
        n //= 10
    return r
""",
    ),
    _task(
        "i07_powmod", "intermediate", "modular",
        "Return (b ^ e) mod m. Guaranteed 0 <= b, 0 <= e <= 2000000000, 1 <= m <= 1000000007. Must be fast for large e.",
        [("b", I64), ("e", I64), ("m", I64)], I64,
        lambda b, e, m: pow(b, e, m),
        [(2, 10, 1000), (3, 0, 7), (0, 5, 13), (7, 123456789, 1000000007), (2, 1000000000, 998244353), (10, 18, 1000000007)],
        """
fn solve(b: i64, e: i64, m: i64) -> i64 {
    let mut result: i64 = 1 % m;
    let mut base: i64 = b % m;
    let mut k: i64 = e;
    while k > 0 {
        if k % 2 == 1 {
            result = result * base % m;
        }
        base = base * base % m;
        k = k / 2;
    }
    return result;
}
""",
        """
def solve(b, e, m):
    result = 1 % m
    base = b % m
    while e > 0:
        if e % 2 == 1:
            result = result * base % m
        base = base * base % m
        e //= 2
    return result
""",
    ),
    _task(
        "i08_sum_squares", "intermediate", "loops",
        "Return 1*1 + 2*2 + ... + n*n. Guaranteed 0 <= n <= 1000.",
        [("n", I32)], I32,
        lambda n: sum(k * k for k in range(1, n + 1)),
        [(0,), (1,), (2,), (10,), (100,), (1000,)],
        """
fn square(x: i32) -> i32 {
    return x * x;
}

fn solve(n: i32) -> i32 {
    let mut total: i32 = 0;
    let mut i: i32 = 1;
    while i <= n {
        total = total + square(i);
        i = i + 1;
    }
    return total;
}
""",
        """
def solve(n):
    return sum(k * k for k in range(1, n + 1))
""",
    ),
    _task(
        "i09_count_divisors", "intermediate", "divisors",
        "Return the number of positive divisors of n. Guaranteed 1 <= n <= 10000.",
        [("n", I32)], I32,
        lambda n: sum(1 for d in range(1, n + 1) if n % d == 0),
        [(1,), (2,), (12,), (97,), (360,), (10000,)],
        """
fn solve(n: i32) -> i32 {
    let mut count: i32 = 0;
    let mut d: i32 = 1;
    while d <= n {
        if n % d == 0 {
            count = count + 1;
        }
        d = d + 1;
    }
    return count;
}
""",
        """
def solve(n):
    return sum(1 for d in range(1, n + 1) if n % d == 0)
""",
    ),
    _task(
        "i10_is_prime", "intermediate", "primes",
        "Return 1 if n is prime, otherwise 0. Guaranteed 0 <= n <= 1000000.",
        [("n", I32)], I32,
        lambda n: _PRIMES[n],
        [(0,), (1,), (97,), (91,), (561,), (999983,)],
        """
fn solve(n: i32) -> i32 {
    if n < 2 {
        return 0;
    }
    let mut d: i32 = 2;
    while d * d <= n {
        if n % d == 0 {
            return 0;
        }
        d = d + 1;
    }
    return 1;
}
""",
        """
def solve(n):
    if n < 2:
        return 0
    d = 2
    while d * d <= n:
        if n % d == 0:
            return 0
        d += 1
    return 1
""",
    ),
    _task(
        "a01_collatz_steps", "advanced", "collatz",
        "Return the number of steps for n to reach 1 under the Collatz rule (even: n/2, odd: 3n+1). Guaranteed 1 <= n <= 1000000; return 0 for n = 1.",
        [("n", I64)], I64,
        _collatz,
        [(1,), (2,), (7,), (27,), (97,), (837799,)],
        """
fn solve(n: i64) -> i64 {
    let mut steps: i64 = 0;
    let mut x: i64 = n;
    while x != 1 {
        if x % 2 == 0 {
            x = x / 2;
        } else {
            x = 3 * x + 1;
        }
        steps = steps + 1;
    }
    return steps;
}
""",
        """
def solve(n):
    steps = 0
    while n != 1:
        n = n // 2 if n % 2 == 0 else 3 * n + 1
        steps += 1
    return steps
""",
    ),
    _task(
        "a02_count_primes", "advanced", "primes",
        "Return the number of primes strictly less than n. Guaranteed 0 <= n <= 20000.",
        [("n", I32)], I32,
        lambda n: sum(_PRIMES[:n]),
        [(0,), (2,), (3,), (10,), (100,), (20000,)],
        """
fn is_prime(n: i32) -> bool {
    if n < 2 {
        return false;
    }
    let mut d: i32 = 2;
    while d * d <= n {
        if n % d == 0 {
            return false;
        }
        d = d + 1;
    }
    return true;
}

fn solve(n: i32) -> i32 {
    let mut count: i32 = 0;
    let mut k: i32 = 2;
    while k < n {
        if is_prime(k) {
            count = count + 1;
        }
        k = k + 1;
    }
    return count;
}
""",
        """
def solve(n):
    count = 0
    for k in range(2, n):
        d = 2
        while d * d <= k and k % d:
            d += 1
        if d * d > k:
            count += 1
    return count
""",
    ),
    _task(
        "a03_isqrt", "advanced", "numeric",
        "Return floor(sqrt(n)) for 0 <= n <= 1000000000000. Floating point does not exist; a linear search is too slow.",
        [("n", I64)], I64,
        math.isqrt,
        [(0,), (1,), (15,), (16,), (999999999999,), (1000000000000,)],
        """
fn solve(n: i64) -> i64 {
    let mut lo: i64 = 0;
    let mut hi: i64 = 1000000;
    while lo < hi {
        let mid: i64 = (lo + hi + 1) / 2;
        if mid * mid <= n {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    return lo;
}
""",
        """
def solve(n):
    lo, hi = 0, 1000000
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if mid * mid <= n:
            lo = mid
        else:
            hi = mid - 1
    return lo
""",
    ),
    _task(
        "a04_power_sum", "advanced", "modular",
        "Return (1^e + 2^e + ... + n^e) mod m. Guaranteed 1 <= n <= 1000, 0 <= e <= 2000000000, 1 <= m <= 1000000007. Must be fast for large e.",
        [("n", I64), ("e", I64), ("m", I64)], I64,
        lambda n, e, m: sum(pow(k, e, m) for k in range(1, n + 1)) % m,
        [(1, 5, 7), (3, 2, 1000), (10, 3, 1000000007), (100, 1000000000, 998244353), (1000, 123456789, 1000000007), (5, 0, 13)],
        """
fn powmod(b: i64, e: i64, m: i64) -> i64 {
    let mut result: i64 = 1 % m;
    let mut base: i64 = b % m;
    let mut k: i64 = e;
    while k > 0 {
        if k % 2 == 1 {
            result = result * base % m;
        }
        base = base * base % m;
        k = k / 2;
    }
    return result;
}

fn solve(n: i64, e: i64, m: i64) -> i64 {
    let mut total: i64 = 0;
    let mut k: i64 = 1;
    while k <= n {
        total = (total + powmod(k, e, m)) % m;
        k = k + 1;
    }
    return total;
}
""",
        """
def solve(n, e, m):
    total = 0
    for k in range(1, n + 1):
        total = (total + pow(k, e, m)) % m
    return total
""",
    ),
    _task(
        "a05_ackermann", "advanced", "recursion",
        "Return the Ackermann function A(m, n): A(0,n)=n+1; A(m,0)=A(m-1,1) for m>0; A(m,n)=A(m-1,A(m,n-1)) for m>0,n>0. Guaranteed 0 <= m <= 3 and 0 <= n <= 6.",
        [("m", I32), ("n", I32)], I32,
        _ackermann,
        [(0, 0), (1, 5), (2, 3), (3, 0), (3, 3), (3, 6)],
        """
fn solve(m: i32, n: i32) -> i32 {
    if m == 0 {
        return n + 1;
    }
    if n == 0 {
        return solve(m - 1, 1);
    }
    return solve(m - 1, solve(m, n - 1));
}
""",
        """
def solve(m, n):
    if m == 0:
        return n + 1
    if n == 0:
        return solve(m - 1, 1)
    return solve(m - 1, solve(m, n - 1))
""",
    ),
    _task(
        "a06_count_perfect", "advanced", "divisors",
        "Return how many perfect numbers lie in [1, n] (a perfect number equals the sum of its proper positive divisors). Guaranteed 1 <= n <= 3000.",
        [("n", I32)], I32,
        _perfect_count,
        [(5,), (6,), (27,), (28,), (500,), (3000,)],
        """
fn divisor_sum(n: i32) -> i32 {
    let mut s: i32 = 0;
    let mut d: i32 = 1;
    while d < n {
        if n % d == 0 {
            s = s + d;
        }
        d = d + 1;
    }
    return s;
}

fn solve(n: i32) -> i32 {
    let mut count: i32 = 0;
    let mut k: i32 = 2;
    while k <= n {
        if divisor_sum(k) == k {
            count = count + 1;
        }
        k = k + 1;
    }
    return count;
}
""",
        """
def solve(n):
    count = 0
    for k in range(2, n + 1):
        if sum(d for d in range(1, k) if k % d == 0) == k:
            count += 1
    return count
""",
    ),
    _task(
        "a07_palindrome_count", "advanced", "digits",
        "Return how many integers k with 1 <= k <= n read the same forwards and backwards in decimal. Guaranteed 1 <= n <= 100000.",
        [("n", I32)], I32,
        lambda n: sum(1 for k in range(1, n + 1) if str(k) == str(k)[::-1]),
        [(9,), (10,), (100,), (1000,), (12345,), (100000,)],
        """
fn reverse(n: i32) -> i32 {
    let mut r: i32 = 0;
    let mut x: i32 = n;
    while x > 0 {
        r = r * 10 + x % 10;
        x = x / 10;
    }
    return r;
}

fn solve(n: i32) -> i32 {
    let mut count: i32 = 0;
    let mut k: i32 = 1;
    while k <= n {
        if reverse(k) == k {
            count = count + 1;
        }
        k = k + 1;
    }
    return count;
}
""",
        """
def solve(n):
    count = 0
    for k in range(1, n + 1):
        r, x = 0, k
        while x:
            r = r * 10 + x % 10
            x //= 10
        count += r == k
    return count
""",
    ),
    _task(
        "a08_binomial", "advanced", "combinatorics",
        "Return the binomial coefficient C(n, k). Guaranteed 0 <= k <= n <= 30. The result fits in i64.",
        [("n", I64), ("k", I64)], I64,
        math.comb,
        [(5, 2), (10, 0), (10, 10), (30, 15), (20, 3), (30, 29)],
        """
fn solve(n: i64, k: i64) -> i64 {
    let mut kk: i64 = k;
    if n - k < kk {
        kk = n - k;
    }
    let mut r: i64 = 1;
    let mut i: i64 = 1;
    while i <= kk {
        r = r * (n - kk + i) / i;
        i = i + 1;
    }
    return r;
}
""",
        """
def solve(n, k):
    k = min(k, n - k)
    r = 1
    for i in range(1, k + 1):
        r = r * (n - k + i) // i
    return r
""",
    ),
    _task(
        "a09_lcm_range", "advanced", "gcd",
        "Return lcm(1, 2, ..., n) mod m. Guaranteed 1 <= n <= 40 and 1 <= m <= 1000000007. The exact lcm fits in i64.",
        [("n", I64), ("m", I64)], I64,
        lambda n, m: math.lcm(*range(1, n + 1)) % m,
        [(1, 1000), (10, 1000000007), (20, 1000), (40, 1000000007), (40, 97), (7, 13)],
        """
fn gcd(a: i64, b: i64) -> i64 {
    if b == 0 {
        return a;
    }
    return gcd(b, a % b);
}

fn solve(n: i64, m: i64) -> i64 {
    let mut l: i64 = 1;
    let mut i: i64 = 2;
    while i <= n {
        l = l / gcd(l, i) * i;
        i = i + 1;
    }
    return l % m;
}
""",
        """
def solve(n, m):
    l = 1
    for i in range(2, n + 1):
        a, b = l, i
        while b:
            a, b = b, a % b
        l = l // a * i
    return l % m
""",
    ),
    _task(
        "a10_totient", "advanced", "numeric",
        "Return Euler's totient phi(n): the count of integers k in [1, n] with gcd(k, n) = 1. Guaranteed 1 <= n <= 1000000.",
        [("n", I32)], I32,
        lambda n: sum(1 for k in range(1, n + 1) if math.gcd(k, n) == 1),
        [(1,), (2,), (36,), (97,), (100000,), (999999,)],
        """
fn solve(n: i32) -> i32 {
    let mut result: i32 = n;
    let mut x: i32 = n;
    let mut p: i32 = 2;
    while p * p <= x {
        if x % p == 0 {
            while x % p == 0 {
                x = x / p;
            }
            result = result - result / p;
        }
        p = p + 1;
    }
    if x > 1 {
        result = result - result / x;
    }
    return result;
}
""",
        """
def solve(n):
    result, x, p = n, n, 2
    while p * p <= x:
        if x % p == 0:
            while x % p == 0:
                x //= p
            result -= result // p
        p += 1
    if x > 1:
        result -= result // x
    return result
""",
    ),
]


def split_of(task):
    for name, families in SPLITS.items():
        if task["family"] in families:
            return name
    raise ValueError(task["family"])
