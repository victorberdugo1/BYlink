import json
import os
import resource
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
BYLC = Path(os.environ.get("BYLC", REPO / "build" / "bylc"))
sys.path.insert(0, str(ROOT / "bench"))
import tasks as task_defs

TASK_INDEX = {t["id"]: t for t in task_defs.TASKS}
COMPILE_TIMEOUT_S = 30
RUN_TIMEOUT_S = 10
MEMORY_LIMIT_BYTES = 1 << 30
FORMATS = ("by", "byai", "py")


def _limits():
    resource.setrlimit(resource.RLIMIT_CPU, (RUN_TIMEOUT_S, RUN_TIMEOUT_S))
    resource.setrlimit(resource.RLIMIT_AS, (MEMORY_LIMIT_BYTES, MEMORY_LIMIT_BYTES))
    resource.setrlimit(resource.RLIMIT_FSIZE, (1 << 20, 1 << 20))
    resource.setrlimit(resource.RLIMIT_NPROC, (64, 64))


def _run_limited(args, timeout, cwd):
    """Run a test program in its own process group and kill the whole group on timeout."""
    proc = subprocess.Popen(
        args,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        cwd=cwd,
        preexec_fn=_limits,
        start_new_session=True,
    )
    try:
        stdout, stderr = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        stdout, stderr = proc.communicate()
        raise subprocess.TimeoutExpired(args, timeout, output=stdout, stderr=stderr)
    return subprocess.CompletedProcess(args, proc.returncode, stdout, stderr)


def _args(values):
    return " ".join(str(v) for v in values)


def harness_main_by(task):
    lines = ["fn main() -> i32 {", "    let mut f: i32 = 0;"]
    for i, args in enumerate(task["tests"]):
        expected = task["ref"](*args)
        call = "solve(" + ", ".join(str(a) for a in args) + ")"
        lines.append(f"    if {call} != {expected} {{")
        lines.append(f"        f = f | {1 << i};")
        lines.append("    }")
    lines += ["    return f;", "}"]
    return "\n".join(lines) + "\n"


def harness_main_byai(task):
    lines = ["fn main > i32", "mut f i32 0"]
    for i, args in enumerate(task["tests"]):
        expected = task["ref"](*args)
        lines.append(f"if ne call solve {_args(args)} {expected}")
        lines.append(f"set f or f {1 << i}")
        lines.append("end")
    lines += ["ret f", "end"]
    return "\n".join(lines) + "\n"


def _classify(stderr):
    for category in ("lexical", "syntax", "type", "semantic", "toolchain"):
        if f"error[{category}]" in stderr:
            return category
    return "unknown"


def _fail(fmt, stage, category, detail, **extra):
    return {
        "format": fmt,
        "stage": stage,
        "compiled": stage != "compile",
        "error_category": category,
        "detail": detail[:500],
        "tests_passed": 0,
        "tests_total": None,
        "passed_all": False,
        **extra,
    }


def _run_native(fmt, source, task):
    with tempfile.TemporaryDirectory(prefix="bnsb_") as tmp:
        tmp = Path(tmp)
        suffix = ".by" if fmt == "by" else ".byai"
        text = source.rstrip("\n") + "\n" + (harness_main_by(task) if fmt == "by" else harness_main_byai(task))
        src = tmp / f"prog{suffix}"
        exe = tmp / "prog"
        src.write_text(text)
        t0 = time.perf_counter()
        try:
            c = subprocess.run([str(BYLC), str(src), "-o", str(exe)], capture_output=True, text=True, timeout=COMPILE_TIMEOUT_S, cwd=tmp)
        except subprocess.TimeoutExpired:
            return _fail(fmt, "compile", "timeout", "compile timeout")
        compile_s = time.perf_counter() - t0
        if c.returncode != 0:
            return _fail(fmt, "compile", _classify(c.stderr), c.stderr, compile_s=compile_s)
        t0 = time.perf_counter()
        try:
            r = _run_limited([str(exe)], RUN_TIMEOUT_S + 5, tmp)
        except subprocess.TimeoutExpired:
            return _fail(fmt, "run", "timeout", "run timeout", compile_s=compile_s)
        run_s = time.perf_counter() - t0
        total = len(task["tests"])
        failed_mask = r.returncode
        if failed_mask < 0 or failed_mask >= 1 << total:
            return _fail(fmt, "run", "crash", f"returncode {r.returncode}", compile_s=compile_s, run_s=run_s)
        passed = total - bin(failed_mask).count("1")
        return {
            "format": fmt,
            "stage": "done",
            "compiled": True,
            "error_category": None,
            "detail": "",
            "tests_passed": passed,
            "tests_total": total,
            "passed_all": passed == total,
            "compile_s": compile_s,
            "run_s": run_s,
        }


PY_DRIVER = r"""
import json, runpy, sys
sys.setrecursionlimit(20000)
spec = json.load(open(sys.argv[2]))
ns = runpy.run_path(sys.argv[1])
fn = ns["solve"]
out = []
for args, expected in spec:
    try:
        out.append(fn(*args) == expected)
    except BaseException:
        out.append(False)
print(json.dumps(out))
"""


def _run_python(source, task):
    with tempfile.TemporaryDirectory(prefix="bnsb_") as tmp:
        tmp = Path(tmp)
        (tmp / "sol.py").write_text(source)
        (tmp / "driver.py").write_text(PY_DRIVER)
        spec = [[list(a), task["ref"](*a)] for a in task["tests"]]
        (tmp / "spec.json").write_text(json.dumps(spec))
        t0 = time.perf_counter()
        try:
            r = _run_limited(
                [sys.executable, "-I", str(tmp / "driver.py"), str(tmp / "sol.py"), str(tmp / "spec.json")],
                RUN_TIMEOUT_S + 5,
                tmp,
            )
        except subprocess.TimeoutExpired:
            return _fail("py", "run", "timeout", "run timeout")
        run_s = time.perf_counter() - t0
        if r.returncode != 0:
            return _fail("py", "run", "crash", r.stderr, run_s=run_s)
        results = json.loads(r.stdout.strip().splitlines()[-1])
        passed = sum(results)
        return {
            "format": "py",
            "stage": "done",
            "compiled": True,
            "error_category": None,
            "detail": "",
            "tests_passed": passed,
            "tests_total": len(results),
            "passed_all": passed == len(results),
            "run_s": run_s,
        }


def reference_source(task, fmt):
    if fmt == "py":
        return task["py"] + "\n"
    by = f"module solution;\n\n{task['by']}\n"
    if fmt == "by":
        return by
    with tempfile.TemporaryDirectory(prefix="bnsb_") as tmp:
        src = Path(tmp) / "ref.by"
        src.write_text(by + harness_main_by(task))
        dst = Path(tmp) / "ref.byai"
        subprocess.run([str(BYLC), str(src), "--emit-byai", "-o", str(dst)], capture_output=True, text=True, check=True)
        out = dst.read_text()
    keep = []
    for line in out.splitlines():
        if line == "fn main > i32":
            break
        keep.append(line)
    return "\n".join(keep) + "\n"


def evaluate(task_id, fmt, source):
    task = TASK_INDEX[task_id]
    if fmt not in FORMATS:
        raise ValueError(fmt)
    if fmt == "py":
        return _run_python(source, task)
    return _run_native(fmt, source, task)


def main(argv):
    task_id, fmt, path = argv
    print(json.dumps(evaluate(task_id, fmt, Path(path).read_text()), indent=2))


if __name__ == "__main__":
    main(sys.argv[1:])
