import collections
import json
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "harness"))
import oracle

TASKS = oracle.task_defs.TASKS


def test_counts_and_tiers():
    assert len(TASKS) == 30
    assert collections.Counter(t["tier"] for t in TASKS) == {"basic": 10, "intermediate": 10, "advanced": 10}
    assert all(len(t["tests"]) == 6 for t in TASKS)


def test_splits_are_family_disjoint_and_complete():
    by_split = collections.defaultdict(set)
    for t in TASKS:
        by_split[oracle.task_defs.split_of(t)].add(t["family"])
    assert not (by_split["train"] & by_split["val"] or by_split["train"] & by_split["test"] or by_split["val"] & by_split["test"])
    assert {t["family"] for t in TASKS} == set().union(*by_split.values())


def test_public_file_does_not_leak():
    public = (ROOT / "bench" / "public" / "tasks.json").read_text()
    data = json.loads(public)
    assert all(set(d) == {"id", "tier", "family", "split", "signature", "spec", "prompt"} for d in data)
    for t in TASKS:
        for line in t["by"].splitlines():
            if len(line.strip()) > 18 and not line.strip().startswith("fn "):
                assert line.strip() not in public
    assert '"expected"' not in public


@pytest.mark.parametrize("fmt", oracle.FORMATS)
def test_all_references_pass(fmt):
    for t in TASKS:
        r = oracle.evaluate(t["id"], fmt, oracle.reference_source(t, fmt))
        assert r["passed_all"], (t["id"], r)


def test_trivial_constant_solution_is_rejected():
    passed = 0
    for t in TASKS:
        params = ", ".join(f"{n}: {ty}" for n, ty in t["params"])
        src = f"module m;\nfn solve({params}) -> {t['ret']} {{\n    return 0;\n}}\n"
        passed += oracle.evaluate(t["id"], "by", src)["passed_all"]
    assert passed == 0


def test_mutants_are_rejected():
    mutations = {
        "b02_abs_diff": ("return b - a;", "return a - b;"),
        "i03_gcd": ("a % b", "a / b"),
        "i07_powmod": ("k / 2", "k / 3"),
        "a03_isqrt": ("mid * mid <= n", "mid * mid < n"),
        "a08_binomial": ("(n - kk + i) / i", "(n - kk + i) / 2"),
        "a10_totient": ("p * p <= x", "p * p < x"),
    }
    for tid, (old, new) in mutations.items():
        src = oracle.reference_source(oracle.TASK_INDEX[tid], "by")
        assert old in src
        assert not oracle.evaluate(tid, "by", src.replace(old, new))["passed_all"], tid


def test_compile_and_runtime_failures_are_classified():
    t = oracle.TASK_INDEX["i03_gcd"]
    src = oracle.reference_source(t, "by")
    assert oracle.evaluate(t["id"], "by", src.replace("return a;", "return a"))["error_category"] == "syntax"
    assert oracle.evaluate(t["id"], "by", src.replace("return solve(b, a % b);", "return 1 / 0;"))["error_category"] == "crash"
    assert oracle.evaluate(t["id"], "by", src.replace("return a;", "return true;"))["error_category"] == "type"
