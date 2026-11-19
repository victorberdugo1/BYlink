import importlib.util
import json
import os
import platform
import shutil
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent


def run(cmd, timeout=20):
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return (r.stdout + r.stderr).strip()[:400], r.returncode
    except Exception as e:
        return repr(e), -1


def cpu_model():
    for line in Path("/proc/cpuinfo").read_text().splitlines():
        if line.startswith("model name"):
            return line.split(":", 1)[1].strip()
    return "unknown"


def mem_gb():
    for line in Path("/proc/meminfo").read_text().splitlines():
        if line.startswith("MemTotal"):
            return round(int(line.split()[1]) / 1048576, 2)


def main():
    disk = shutil.disk_usage(HERE)
    modules = {m: importlib.util.find_spec(m) is not None for m in ("torch", "transformers", "tokenizers", "peft", "accelerate", "safetensors", "huggingface_hub", "numpy", "scipy", "sklearn", "onnxruntime", "llama_cpp")}
    probes = {}
    probes["huggingface.co"] = run(["curl", "-sS", "-m", "10", "-o", "/dev/null", "-w", "%{http_code}", "https://huggingface.co/api/models/Qwen/Qwen2.5-Coder-0.5B"])
    probes["pypi_torch"] = run([sys.executable, "-m", "pip", "download", "torch", "--no-deps", "-d", "/tmp/bnsb_pip_probe"], timeout=60)
    out = {
        "kind": "environment_audit",
        "timestamp_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "cpu_model": cpu_model(),
        "cpu_count": os.cpu_count(),
        "ram_gb": mem_gb(),
        "disk_total_gb": round(disk.total / 1e9, 1),
        "disk_free_gb": round(disk.free / 1e9, 1),
        "gpu": {"nvidia_smi": shutil.which("nvidia-smi"), "cuda_visible": os.environ.get("CUDA_VISIBLE_DEVICES")},
        "os": platform.platform(),
        "python": platform.python_version(),
        "gxx": run(["g++", "--version"])[0].splitlines()[0],
        "cmake": run(["cmake", "--version"])[0].splitlines()[0],
        "llvm": run(["llvm-config", "--version"])[0],
        "python_modules_present": modules,
        "hf_cache_present": Path.home().joinpath(".cache/huggingface").exists(),
        "local_model_servers": {"ollama": shutil.which("ollama"), "llama_server": shutil.which("llama-server")},
        "network_probes": {k: {"output": v[0], "returncode": v[1]} for k, v in probes.items()},
    }
    (HERE / "results").mkdir(exist_ok=True)
    (HERE / "results" / "environment.json").write_text(json.dumps(out, indent=1))
    print(json.dumps(out, indent=1))


if __name__ == "__main__":
    main()
