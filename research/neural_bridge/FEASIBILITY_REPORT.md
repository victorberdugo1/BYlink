# INCONCLUSIVE

The GO / NO-GO question for BNSR could not be answered in this environment. No model weights, no ML framework and no GPU were available, so Experiments A–D and the model-dependent parts of E were not run. **No result about H1, H2 or H3 exists, positive or negative.** What was built and verified is the benchmark, the compiler oracle, the alignment/gate code, and the accounting/decision code, so the experiment can be finished unchanged once the blocker below is removed.

**Smallest missing resource:** allowlisted network access from the execution environment to the Hugging Face Hub and to PyPI (`torch`, `transformers`, `tokenizers`, `peft`). Two ≤1.5B open-weight teachers can then be run for hidden-state extraction on the existing 2 CPUs / 7.8 GB RAM (bf16). Training the student (Experiment C) is realistically only practical with a GPU; that is an estimate, not a measurement.

---

## 1. Research hypothesis
H1 shared representations across independent LLM families; H2 transferable into a smaller student without teachers at runtime; H3 practical advantage (payload, cost, latency) when the student emits a learned representation (BNSR) decoded into BYlink AST/IR. Tested separately, as specified. Status: **H1 untested, H2 untested, H3 untested.**

## 2. Existing compiler capabilities and limitations
Inspected and built from the supplied repository (`bylc` 0.1.0, language 0.1, IR version 0, LLVM 18.1.3, g++ 13.3, CMake 3.28). Existing test suite: **69 passed, 0 failed, 1 skipped**, before and after this work. No compiler, AST, IR, BYAI, BYB or backend file was modified; everything is under `research/neural_bridge/`.
Language limits (from `docs/LANGUAGE_SPECIFICATION.md`): integers and `bool` only, no I/O, arrays, pointers, structs, strings, floats, shifts, multi-file, threads; observable output is `main`'s exit code (8 bits on Linux). The benchmark therefore measures the expressible subset only, not application development. No `.byc` frontend exists in the supplied repository and none was assumed.
Observation relevant to H3: the BYlink IR and BYAI are already compact text interfaces. On the 30 experimenter-written reference solutions (not model output) the Python version is smaller than BYAI by every unit measured (table in §9).

## 3. Models and hardware
| Item | Value |
|---|---|
| CPU | Intel Xeon @ 2.10 GHz, 2 cores |
| RAM | 7.84 GB |
| GPU / CUDA | none (`nvidia-smi` absent) |
| Disk free | 32.2 GB |
| Python | 3.13.18; numpy 2.5.3, scipy 1.18.1, scikit-learn 1.9.1, onnxruntime 1.29.0 |
| torch / transformers / tokenizers / peft / llama.cpp | **absent** |
| Cached weights, local model servers | none |
| Hugging Face Hub | blocked (HTTP 403 at proxy) |
| PyPI | unreachable (name resolution fails); `pip download torch` fails |

Teacher and student models were **not selected or downloaded**: availability could not be confirmed and installing a framework was impossible. Candidate plan (unverified, not downloaded): two or three ≤1.5B code-capable open-weight models from different families as teachers, and a ≤1.5B student; the 1.5B student target is **untested**. Raw audit: `results/environment.json` (`audit_env.py`).

## 4. Scientific literature reviewed
Only the three supplied leads could be fetched (abstract pages only; full texts, code and limitations sections were not read). Other topics in the brief (Platonic Representation Hypothesis, CKA, SAEs, crosscoders, VQ) were **not verified this session**; a fetch of the PRH arXiv page was refused by the allowlist.

| Lead | What the abstract page states | Relevance / caution |
|---|---|---|
| arXiv 2610.19238, Christensen et al., v1, 13 Oct 2026 | Code-generating LLMs encode function, language and strategy separably in middle layers; steering can combine them; fine-tuning transfer across languages. | Supports looking at middle layers within one model; says nothing about cross-model alignment. No code link seen. |
| arXiv 2608.16347, Cardenas Piepereit, v1, 17 Aug 2026 | Learned projection transfers activations between models. Top-1 retrieval 45–50% vs 5% chance on three decoder-only pairs, none for FLAN-T5. Only 1 of 3 decoder pairs showed a significant causal effect (p=0.047, FDR-corrected). "Architecture-dependent, not universal." | Closest prior. Its retrieval numbers are **below** this study's pre-registered 60% threshold (setting and candidate pool differ). Causal transfer was weak. No code link seen. |
| arXiv 2608.05164, Agarwal, v1 | Cross-model steering transfer works only above a capacity threshold (~1.7B, described as "suggestive"); degrades below it. | A ≤1.5B student sits below that suggested threshold. Anomaly: page says submitted 26 May 2026 while the ID prefix 2608 implies August; unresolved. No code link seen. |

All three are single-version preprints, treated as unconfirmed leads. Prior from these: signal is plausible for retrieval, doubtful for causal transfer and for small models. This is a prior, not a result.

## 5. Alignment experiment results (Experiment A)
**Not run** (no models). Built and verified instead: `harness/alignment.py` implements linear CKA, RSA (Spearman over RDMs), mutual k-NN, Procrustes and regularised CCA aligners, cross-model top-1 retrieval, 95% bootstrap CI, permutation baseline, shuffled-pair and random-vector controls, validation-only selection of depth/method/rank, and the frozen Gate A check. Frozen constants: top-1 ≥ 0.60; CI lower bound > permutation p95; ≥ 3 seeds; ≥ 2 model families; ≥ 100 held-out anchors from ≥ 5 held-out families (the last three were added **before** any result existed, to make "sufficiently diverse" operational).
Harness validation on **synthetic data only** (not evidence about LLMs), `tests/test_alignment.py`, 7/7 pass: identity gives CKA/RSA/kNN = 1; an exact orthogonal rotation is recovered at 100% top-1; a shared-latent positive control passes the gate; unrelated features, heavily noisy signal, an under-sized anchor set and a single model family all fail it; shuffled and random controls sit at chance.
`harness/extract_hidden.py` (mean-pooled hidden states at fractional depths 0.25–1.0) is written but **untested** because `torch` is missing.

## 6. Cross-model transfer results (Experiment D)
Not run. Requires models, and for incompatible architectures a trained adapter. No data.

## 7. Student training results (Experiments B and C)
Not run. No BNSR representation, codebook, student or control student exists. Nothing here may be read as a measurement.

## 8. Program generation comparison (Experiment E)
Benchmark and oracle are complete; **no model was evaluated**. All results below are for experimenter-written reference solutions and validate the oracle only.

| System | Pass@1 held-out | Status |
|---|---|---|
| A Direct BYlink (student) | n/a | not run, no model |
| B Direct BYAI (student) | n/a | not run |
| C Python (student) | n/a | not run |
| D Ordinary distillation | n/a | not run |
| E BNSR | n/a | not run |
| F Teacher upper bound | n/a | not run |
| Reference solutions, BYlink / BYAI / Python | 30/30, 30/30, 30/30 (all 6 hidden tests each) | measured: `results/reference_validation.json` |

Benchmark: 30 tasks, 10 basic / 10 intermediate / 10 advanced, each with a natural-language spec, an independent Python reference (stdlib `math`/`calendar` or closed forms where possible), 6 hidden tests, tier and family id. Splits by algorithm family: train {arith, cond, loops, digits} 13 tasks; val {recursion, divisors, numeric} 6; **test {gcd, primes, modular, collatz, combinatorics} 11**. The train split is basic-heavy (8 basic of 13), a known imbalance. Tasks needing composition (e.g. `lcm_range`→gcd, `power_sum`→powmod, `count_primes`→is_prime, `palindrome_count`→reverse) are in train/val/test.
Oracle (`harness/oracle.py`): the student supplies a module without `main`; the harness appends a `main` returning a bitmask of failed tests, compiles with `bylc prog.{by,byai} -o prog`, and runs with CPU/memory/file/process rlimits in a temp dir with a timeout. Python runs under `python3 -I` with the same limits and tests. Isolation is process-level only (no network namespace or container), which is a stated limitation. Oracle checks (`tests/test_benchmark.py`, 9/9): constant-return solutions pass 0/30; six single-token mutants of reference solutions all fail; compile-error, type-error and crash classification; public task file leaks no tests or reference code.
Statistical caveat: only 11 held-out tasks. A 95% CI on pass@1 will be about ±25–30 points wide, so the pre-registered "within 5 points of the best baseline" and "≥30% payload reduction with CI support" criteria cannot be established on this suite alone. More held-out tasks (parametric variants per family) and multiple samples per task are needed.

## 9. Token accounting
**No LLM tokens were counted** and none are estimated: no model tokenizers are installed (`bench/tokens/measure.mjs` needs npm packages that cannot be installed here). Reference-solution sizes below are bytes and whitespace words, **not tokens**, and are not model output.

| Format | Mean bytes / task | Mean words / task | Mean lines / task |
|---|---:|---:|---:|
| BYlink `.by` (incl. `module` header) | 225.4 | 48.5 | 13.3 |
| BYAI | 166.7 | 44.5 | 13.1 |
| Python | 105.1 | 23.2 | 4.7 |

BYAI is smaller than `.by` but larger than Python on these hand-written references. Whether model tokens follow the same ordering is unmeasured.

## 10. Serialized payload accounting
Formulas are implemented and unit-tested in `harness/accounting.py` (`continuous_payload_bytes = values × bytes_per_value + header + index + metadata`; `discrete_payload_bytes = ceil(symbols × ceil(log2 K) / 8) + header + metadata`). Tokens and bytes are kept separate. No BNSR payload exists, so no BNSR number is reported.

## 11. Cost per verified success
`cost_per_verified_success` (training cost amortised over 100 / 1,000 / 10,000 tasks plus inference, divided by pass rate) is implemented and tested. No training or inference cost was measured. Not reported.

## 12. Latency and memory
Not measured for any model. Only compile/run timings of reference programs are stored (`compile_s`, `run_s` per row in the validation JSON; each compile ≈ 0.01–0.1 s).

## 13. Hidden-test results
Reference solutions only (see §8). No student output was ever scored, so there is no student leakage question yet. Hidden tests and references live in `bench/hidden/`, public prompts in `bench/public/`; a model runner must read only `bench/public/tasks.json`.

## 14. Ablation studies
None run. Planned: BNSR vs ordinary distillation with matched examples and budget (the decisive comparison), layer depth, continuous vs discrete, random/shuffled/same-model interventions.

## 15. Statistical confidence
Machine-checked only for the harness (bootstrap CI and permutation baseline behave correctly on synthetic controls). No experimental confidence intervals exist.

## 16. Reproduction instructions
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
./build/bylink_tests                                  # 69 passed, 1 skipped
cd research/neural_bridge
python3 -I -m pytest -q tests                         # 20 passed
python3 -I bench/build_bench.py                       # regenerates public/hidden/reference
python3 -I validate_reference.py                      # 90/90 references pass
python3 -I audit_env.py                               # environment.json
```
To finish the study after unblocking: pin and install `torch transformers tokenizers peft`; extract hidden states with `harness/extract_hidden.py` for ≥ 2 families over ≥ 100 anchors (NL spec, Python and BYlink views of each task and its parametric variants) from ≥ 5 held-out families; run `alignment.run_pair` + `gate_a`; only if Gate A passes proceed to BNSR, student, control student and baselines A–F; fill the metrics dict and call `accounting.go_decision`.

## 17. Raw result locations
`research/neural_bridge/results/environment.json`, `results/reference_validation.json`, `bench/public/tasks.json`, `bench/hidden/tests.json`, `bench/hidden/reference/{by,byai,py}/`.

## 18. Reasons to continue or stop
No evidence either way. Observations that do not decide the question but should shape the next attempt:
- Literature prior is mixed: retrieval alignment is reported at 45–50% top-1 (below the 60% gate), causal transfer weak (1 of 3 pairs), and transfer reported to degrade near and below ~1.7B parameters, where this study's student sits.
- The decisive control is ordinary distillation on the same teacher data; without it any BNSR gain is unattributable.
- BYAI is already a compact interface; BNSR must beat it and Python on payload per verified success, not just beat verbose `.by`.
- The 30-task suite is too small to certify the 5-point and 30% criteria; enlarge the held-out set before spending training compute.

Decision rules are frozen in `harness/accounting.py` (`go_decision`) and `harness/alignment.py` (`gate_a`). This work does not show anything about general intelligence or arbitrary application generation.
