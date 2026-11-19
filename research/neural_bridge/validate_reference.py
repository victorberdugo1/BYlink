import json
import platform
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "harness"))
import oracle

RESULTS = HERE / "results"


def main():
    rows = []
    for task in oracle.TASK_INDEX.values():
        for fmt in oracle.FORMATS:
            source = oracle.reference_source(task, fmt)
            result = oracle.evaluate(task["id"], fmt, source)
            rows.append(
                {
                    "task": task["id"],
                    "tier": task["tier"],
                    "family": task["family"],
                    "split": oracle.task_defs.split_of(task),
                    "source_bytes": len(source.encode()),
                    "source_lines": source.count("\n"),
                    "source_words": len(source.split()),
                    **result,
                }
            )
    version = subprocess.run([str(oracle.BYLC), "--version"], capture_output=True, text=True).stdout.strip()
    out = {
        "kind": "reference_validation",
        "note": "Reference solutions written by the experimenter, not model output. Validates benchmark and oracle only.",
        "compiler_version": version,
        "bylc_command": "bylc prog.{by,byai} -o prog",
        "python": platform.python_version(),
        "timestamp_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "rows": rows,
    }
    RESULTS.mkdir(exist_ok=True)
    (RESULTS / "reference_validation.json").write_text(json.dumps(out, indent=1))
    failures = [r for r in rows if not r["passed_all"]]
    print(f"{len(rows) - len(failures)}/{len(rows)} reference solutions pass all hidden tests")
    for r in failures:
        print("FAIL", r["task"], r["format"], r["stage"], r["error_category"], r["detail"][:200])
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
