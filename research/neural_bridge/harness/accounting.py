import math

import numpy as np

PAYLOAD_REDUCTION_MIN = 0.30
COST_REDUCTION_MIN = 0.20
LATENCY_RATIO_MAX = 1.5
PASS_AT_1_GAP_MAX = 0.05
AMORTIZATION_VOLUMES = (100, 1000, 10000)


def continuous_payload_bytes(n_values, bytes_per_value, header_bytes=0, index_bytes=0, metadata_bytes=0):
    return n_values * bytes_per_value + header_bytes + index_bytes + metadata_bytes


def discrete_payload_bytes(n_symbols, codebook_size, header_bytes=0, metadata_bytes=0):
    bits = n_symbols * max(1, math.ceil(math.log2(codebook_size)))
    return math.ceil(bits / 8) + header_bytes + metadata_bytes


def cost_per_verified_success(training_cost, n_tasks, inference_cost_per_task, pass_rate):
    if pass_rate <= 0:
        return math.inf
    return (training_cost / n_tasks + inference_cost_per_task) / pass_rate


def bootstrap_ratio_ci(numer, denom, rng, n=2000, level=0.95):
    numer, denom = np.asarray(numer, float), np.asarray(denom, float)
    idx = rng.integers(0, len(numer), size=(n, len(numer)))
    ratios = numer[idx].sum(1) / denom[idx].sum(1)
    return tuple(float(x) for x in np.quantile(ratios, [(1 - level) / 2, 1 - (1 - level) / 2]))


def go_decision(m):
    checks = {
        "A_alignment_gate_passed": m.get("alignment_gate_passed") is True,
        "B_student_runs_without_teachers": m.get("teachers_required_at_runtime") is False,
        "B_decodes_to_valid_bylink": m.get("decoder_valid_rate_ci_low", 0) > 0.0 and m.get("decoder_valid_rate", 0) >= 0.95,
        "B_generalises_to_heldout_families": m.get("heldout_family_pass_at_1", 0) > 0,
        "C_pass_within_5pp_of_best_direct": m.get("pass_at_1_gap_to_best_direct", 1) <= PASS_AT_1_GAP_MAX,
        "C_composition_demonstrated": m.get("composition_tasks_solved", 0) > 0,
        "C_no_leakage": m.get("hidden_test_leakage") is False,
        "D_payload_reduction_ci_low": m.get("payload_reduction_ci_low", 0) >= PAYLOAD_REDUCTION_MIN,
        "D_cost_reduction_ci_low": m.get("cost_reduction_ci_low", 0) >= COST_REDUCTION_MIN,
        "D_latency_ratio_ci_high": m.get("latency_ratio_ci_high", math.inf) <= LATENCY_RATIO_MAX,
        "D_beats_ordinary_distillation": m.get("beats_ordinary_distillation") is True,
        "E_reproducible_from_artifacts": m.get("reproduced") is True,
    }
    return {"decision": "GO" if all(checks.values()) else "NOT_GO", "checks": checks}
