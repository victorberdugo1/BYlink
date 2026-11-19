import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "harness"))
import oracle

tasks = oracle.task_defs


def signature(task):
    params = ", ".join(f"{n}: {t}" for n, t in task["params"])
    return f"fn solve({params}) -> {task['ret']}"


def main():
    public, hidden = [], []
    for t in tasks.TASKS:
        public.append(
            {
                "id": t["id"],
                "tier": t["tier"],
                "family": t["family"],
                "split": tasks.split_of(t),
                "signature": signature(t),
                "spec": t["spec"],
                "prompt": (
                    f"Implement `{signature(t)}`. {t['spec']} "
                    "Define only helper functions and `solve`; do not define `main`. "
                    "Report nothing else: there is no I/O."
                ),
            }
        )
        hidden.append(
            {
                "id": t["id"],
                "tests": [{"args": list(a), "expected": t["ref"](*a)} for a in t["tests"]],
            }
        )
    (HERE / "public").mkdir(exist_ok=True)
    (HERE / "hidden").mkdir(exist_ok=True)
    (HERE / "public" / "tasks.json").write_text(json.dumps(public, indent=1))
    (HERE / "hidden" / "tests.json").write_text(json.dumps(hidden, indent=1))
    for fmt, ext in (("by", "by"), ("byai", "byai"), ("py", "py")):
        d = HERE / "hidden" / "reference" / fmt
        d.mkdir(parents=True, exist_ok=True)
        for t in tasks.TASKS:
            (d / f"{t['id']}.{ext}").write_text(oracle.reference_source(t, fmt))
    print(f"{len(public)} tasks written")


if __name__ == "__main__":
    main()
