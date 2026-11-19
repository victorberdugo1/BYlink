import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "harness"))
import alignment as al


def synthetic(noise, n=420, d_latent=12, seed=0, shared=True):
    rng = np.random.default_rng(seed)
    centres = rng.standard_normal((14, d_latent)) * 2
    fam = rng.integers(0, 14, n)
    z = centres[fam] + rng.standard_normal((n, d_latent))
    wa = rng.standard_normal((d_latent, 48))
    wb = rng.standard_normal((d_latent, 80))
    a = np.tanh(z @ wa / 3) + noise * rng.standard_normal((n, 48))
    zb = z if shared else rng.standard_normal((n, d_latent))
    b = np.tanh(zb @ wb / 3) + noise * rng.standard_normal((n, 80))
    split = np.array(["train"] * 240 + ["val"] * 60 + ["test"] * 120)
    return {"d": a}, {"d": b}, split


def test_metrics_identity_and_independence():
    rng = np.random.default_rng(1)
    X = rng.standard_normal((200, 30))
    assert al.linear_cka(X, X) == pytest.approx(1.0)
    assert al.rsa(X, X) == pytest.approx(1.0)
    assert al.mutual_knn(X, X, 5) == pytest.approx(1.0)
    Y = rng.standard_normal((200, 30))
    assert al.linear_cka(X, Y) < 0.3
    assert abs(al.rsa(X, Y)) < 0.1


def test_orthogonal_rotation_recovered_exactly():
    rng = np.random.default_rng(2)
    X = rng.standard_normal((300, 16))
    Q, _ = np.linalg.qr(rng.standard_normal((16, 16)))
    Y = X @ Q
    a = al.Aligner("procrustes", 16).fit(X[:200], Y[:200])
    assert al.hits_top1(a.project_x(X[200:]), a.project_y(Y[200:])).mean() == 1.0


def test_positive_control_passes_gate():
    fa, fb, split = synthetic(noise=0.05)
    res = al.run_pair(fa, fb, split, seeds=[0, 1, 2])
    gate = al.gate_a(res, n_test_families=5, model_families=["a", "b"])
    assert gate["passed"], gate["reasons"]


def test_unrelated_models_fail_gate():
    fa, fb, split = synthetic(noise=0.05, shared=False)
    res = al.run_pair(fa, fb, split, seeds=[0, 1, 2])
    gate = al.gate_a(res, n_test_families=5, model_families=["a", "b"])
    assert not gate["passed"]
    assert all(r["test"]["top1"] < 0.2 for r in res.values())


def test_noisy_shared_signal_fails_threshold():
    fa, fb, split = synthetic(noise=3.0)
    res = al.run_pair(fa, fb, split, seeds=[0, 1, 2])
    assert not al.gate_a(res, 5, ["a", "b"])["passed"]


def test_controls_at_chance_on_real_signal():
    fa, fb, split = synthetic(noise=0.05)
    res = al.run_pair(fa, fb, split, seeds=[0])[0]
    for c in res["controls"].values():
        assert c["top1"] < 0.1


def test_small_benchmark_cannot_pass_gate():
    fa, fb, split = synthetic(noise=0.05)
    res = al.run_pair(fa, fb, split, seeds=[0, 1, 2])
    gate = al.gate_a(res, n_test_families=2, model_families=["a"])
    assert not gate["passed"]
