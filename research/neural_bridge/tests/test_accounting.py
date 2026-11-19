import math
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "harness"))
import accounting as ac


def test_payload_formulas():
    assert ac.continuous_payload_bytes(256, 2, header_bytes=16, index_bytes=4, metadata_bytes=8) == 540
    assert ac.discrete_payload_bytes(100, 1024, header_bytes=8) == 133
    assert ac.discrete_payload_bytes(1, 1) == 1


def test_cost_per_success():
    assert ac.cost_per_verified_success(1000, 100, 1, 0.5) == 22
    assert ac.cost_per_verified_success(1000, 100, 1, 0) == math.inf


def test_ratio_ci_contains_point_estimate():
    rng = np.random.default_rng(0)
    n, d = np.arange(1, 31), np.arange(31, 61)
    lo, hi = ac.bootstrap_ratio_ci(n, d, rng)
    assert lo <= n.sum() / d.sum() <= hi


def test_go_requires_every_check():
    full = {
        "alignment_gate_passed": True, "teachers_required_at_runtime": False,
        "decoder_valid_rate": 0.99, "decoder_valid_rate_ci_low": 0.9,
        "heldout_family_pass_at_1": 0.3, "pass_at_1_gap_to_best_direct": 0.02,
        "composition_tasks_solved": 3, "hidden_test_leakage": False,
        "payload_reduction_ci_low": 0.4, "cost_reduction_ci_low": 0.25, "latency_ratio_ci_high": 1.2,
        "beats_ordinary_distillation": True, "reproduced": True,
    }
    assert ac.go_decision(full)["decision"] == "GO"
    for key in full:
        broken = dict(full)
        broken[key] = None if isinstance(full[key], bool) else 0
        if key == "latency_ratio_ci_high":
            broken[key] = 9
        if key == "pass_at_1_gap_to_best_direct":
            broken[key] = 0.5
        assert ac.go_decision(broken)["decision"] == "NOT_GO", key
    assert ac.go_decision({})["decision"] == "NOT_GO"
