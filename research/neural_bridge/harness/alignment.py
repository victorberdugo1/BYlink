import numpy as np
from scipy.stats import rankdata

TOP1_MIN = 0.60
CI_LEVEL = 0.95
N_TEST_MIN = 100
TEST_FAMILIES_MIN = 5
SEEDS_MIN = 3
MODEL_FAMILIES_MIN = 2
N_PERMUTATIONS = 1000
N_BOOTSTRAP = 2000
METHODS = ("procrustes", "cca")
RANKS = (8, 16, 32, 64)


def _center(X, mean=None):
    mean = X.mean(0) if mean is None else mean
    return X - mean, mean


def linear_cka(X, Y):
    X, _ = _center(X)
    Y, _ = _center(Y)
    cross = np.linalg.norm(X.T @ Y, "fro") ** 2
    return float(cross / (np.linalg.norm(X.T @ X, "fro") * np.linalg.norm(Y.T @ Y, "fro")))


def _rdm(X):
    X, _ = _center(X)
    X = X / (np.linalg.norm(X, axis=1, keepdims=True) + 1e-12)
    return 1.0 - X @ X.T


def rsa(X, Y):
    iu = np.triu_indices(len(X), 1)
    a = rankdata(_rdm(X)[iu])
    b = rankdata(_rdm(Y)[iu])
    return float(np.corrcoef(a, b)[0, 1])


def mutual_knn(X, Y, k=10):
    def neighbours(Z):
        Z, _ = _center(Z)
        Z = Z / (np.linalg.norm(Z, axis=1, keepdims=True) + 1e-12)
        S = Z @ Z.T
        np.fill_diagonal(S, -np.inf)
        return np.argsort(-S, axis=1)[:, :k]

    na, nb = neighbours(X), neighbours(Y)
    overlap = [len(set(a) & set(b)) / k for a, b in zip(na, nb)]
    return float(np.mean(overlap))


def _pca(X, k):
    Xc, mean = _center(X)
    _, _, vt = np.linalg.svd(Xc, full_matrices=False)
    k = min(k, vt.shape[0])
    return mean, vt[:k].T


def _inv_sqrt(C):
    w, v = np.linalg.eigh(C)
    return (v / np.sqrt(np.maximum(w, 1e-12))) @ v.T


class Aligner:
    def __init__(self, method, rank, ridge=1e-3):
        if method not in METHODS:
            raise ValueError(method)
        self.method, self.rank, self.ridge = method, rank, ridge

    def fit(self, X, Y):
        self.mx, self.px = _pca(X, self.rank)
        self.my, self.py = _pca(Y, self.rank)
        A = (X - self.mx) @ self.px
        B = (Y - self.my) @ self.py
        k = min(A.shape[1], B.shape[1])
        A, B = A[:, :k], B[:, :k]
        self.px, self.py = self.px[:, :k], self.py[:, :k]
        if self.method == "procrustes":
            u, _, vt = np.linalg.svd(A.T @ B)
            self.wx, self.wy = u @ vt, np.eye(k)
        else:
            n = len(A)
            Caa = A.T @ A / n + self.ridge * np.eye(k)
            Cbb = B.T @ B / n + self.ridge * np.eye(k)
            Wa, Wb = _inv_sqrt(Caa), _inv_sqrt(Cbb)
            u, _, vt = np.linalg.svd(Wa @ (A.T @ B / n) @ Wb)
            self.wx, self.wy = Wa @ u, Wb @ vt.T
        return self

    def project_x(self, X):
        return (X - self.mx) @ self.px @ self.wx

    def project_y(self, Y):
        return (Y - self.my) @ self.py @ self.wy


def hits_top1(Q, C):
    Q = Q / (np.linalg.norm(Q, axis=1, keepdims=True) + 1e-12)
    C = C / (np.linalg.norm(C, axis=1, keepdims=True) + 1e-12)
    return (np.argmax(Q @ C.T, axis=1) == np.arange(len(Q))).astype(float)


def bootstrap_ci(hits, rng, n=N_BOOTSTRAP, level=CI_LEVEL):
    idx = rng.integers(0, len(hits), size=(n, len(hits)))
    means = hits[idx].mean(1)
    lo, hi = np.quantile(means, [(1 - level) / 2, 1 - (1 - level) / 2])
    return float(lo), float(hi)


def permutation_baseline(Q, C, rng, n=N_PERMUTATIONS):
    Q = Q / (np.linalg.norm(Q, axis=1, keepdims=True) + 1e-12)
    C = C / (np.linalg.norm(C, axis=1, keepdims=True) + 1e-12)
    best = np.argmax(Q @ C.T, axis=1)
    scores = np.empty(n)
    for i in range(n):
        scores[i] = float((best == rng.permutation(len(Q))).mean())
    return scores


def evaluate_config(Xtr, Ytr, Xte, Yte, method, rank, seed, direction="a_to_b"):
    rng = np.random.default_rng(seed)
    al = Aligner(method, rank).fit(Xtr, Ytr)
    Q, C = (al.project_x(Xte), al.project_y(Yte))
    if direction == "b_to_a":
        Q, C = C, Q
    hits = hits_top1(Q, C)
    base = permutation_baseline(Q, C, rng)
    lo, hi = bootstrap_ci(hits, rng)
    return {
        "top1": float(hits.mean()),
        "ci_low": lo,
        "ci_high": hi,
        "perm_mean": float(base.mean()),
        "perm_p95": float(np.quantile(base, 0.95)),
        "perm_p": float((np.sum(base >= hits.mean()) + 1) / (len(base) + 1)),
        "n_test": int(len(hits)),
    }


def controls(Xtr, Ytr, Xte, Yte, method, rank, seed):
    rng = np.random.default_rng(seed + 10_000)
    shuffled = evaluate_config(Xtr, Ytr[rng.permutation(len(Ytr))], Xte, Yte, method, rank, seed)
    noise = evaluate_config(Xtr, rng.standard_normal(Ytr.shape), Xte, rng.standard_normal(Yte.shape), method, rank, seed)
    return {"shuffled_train_pairs": shuffled, "random_vectors": noise}


def select_config(Xtr, Ytr, Xva, Yva, seed):
    best = None
    for method in METHODS:
        for rank in RANKS:
            top1 = evaluate_config(Xtr, Ytr, Xva, Yva, method, rank, seed)["top1"]
            if best is None or top1 > best[0]:
                best = (top1, method, rank)
    return {"val_top1": best[0], "method": best[1], "rank": best[2]}


def run_pair(feats_a, feats_b, split, seeds):
    results = {}
    for seed in seeds:
        rng = np.random.default_rng(seed)
        tr = np.flatnonzero(split == "train")
        tr = rng.choice(tr, size=int(0.8 * len(tr)), replace=False)
        va, te = np.flatnonzero(split == "val"), np.flatnonzero(split == "test")
        per_depth = {}
        for depth in feats_a:
            cfg = select_config(feats_a[depth][tr], feats_b[depth][tr], feats_a[depth][va], feats_b[depth][va], seed)
            per_depth[depth] = cfg
        depth = max(per_depth, key=lambda d: per_depth[d]["val_top1"])
        cfg = per_depth[depth]
        Xtr, Ytr = feats_a[depth][tr], feats_b[depth][tr]
        Xte, Yte = feats_a[depth][te], feats_b[depth][te]
        results[seed] = {
            "selected_depth": depth,
            "selected": cfg,
            "test": evaluate_config(Xtr, Ytr, Xte, Yte, cfg["method"], cfg["rank"], seed),
            "controls": controls(Xtr, Ytr, Xte, Yte, cfg["method"], cfg["rank"], seed),
            "cka_test": linear_cka(Xte, Yte),
            "rsa_test": rsa(Xte, Yte),
            "mutual_knn_test": mutual_knn(Xte, Yte),
        }
    return results


def gate_a(pair_results, n_test_families, model_families):
    reasons = []
    seeds = list(pair_results.values())
    if len(seeds) < SEEDS_MIN:
        reasons.append(f"seeds {len(seeds)} < {SEEDS_MIN}")
    if min(s["test"]["n_test"] for s in seeds) < N_TEST_MIN:
        reasons.append(f"held-out anchors < {N_TEST_MIN}")
    if n_test_families < TEST_FAMILIES_MIN:
        reasons.append(f"held-out families {n_test_families} < {TEST_FAMILIES_MIN}")
    if len(set(model_families)) < MODEL_FAMILIES_MIN:
        reasons.append(f"model families {len(set(model_families))} < {MODEL_FAMILIES_MIN}")
    for seed, s in pair_results.items():
        t = s["test"]
        if t["top1"] < TOP1_MIN:
            reasons.append(f"seed {seed}: top1 {t['top1']:.3f} < {TOP1_MIN}")
        if t["ci_low"] <= t["perm_p95"]:
            reasons.append(f"seed {seed}: CI lower bound {t['ci_low']:.3f} <= permutation p95 {t['perm_p95']:.3f}")
        for name, c in s["controls"].items():
            if c["top1"] >= t["top1"]:
                reasons.append(f"seed {seed}: control {name} not below real alignment")
    return {"passed": not reasons, "reasons": reasons}
