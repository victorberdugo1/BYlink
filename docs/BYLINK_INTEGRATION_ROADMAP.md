# Integration Roadmap

## Architecture diagram

```
 LLM / human                                                        PUBLIC |  PROPRIETARY
 -----------                                                        -------+--------------
 .by source ----------------------------+
 .byai  (AI text, Tech A) --------------+--> AST --> sema --> BYlink IR   |  (slot IR, text-printable, .byb)
 .byb   (binary IR, Tech A) ---------------------------------> BYlink IR   |
                                                                  |        |
                                              IR optimizer (O1+)  |        |  future: feature-aware passes
                                                                  v        |
 .bypia (portable ISA text, Tech B) ---------------------> BPIA module     |  (SSA, memory regions, traps)
                                                                  |        |
                                               BPIA verifier ---->|        |
                                                                  v        |
                                       LLVM IR text (owned lowering rules) |
                                                                  v        |
                                              clang -O<n> --target --cpu   |  (external toolchain)
                                                                  v        |
                                           native object / executable      |
                                                                  v        |
              BYX container: metadata + SHA-256 + Ed25519 (Tech C) ------> signed artifact
              License file: Ed25519-signed entitlements ---------------> verifier in bylc / service

 Public:  language spec, IR text spec, BPIA spec, AI protocol, BYX format spec, verifier semantics
 Private: compiler source, optimizer passes, cloud service, license issuing keys, signing keys
```

Dependency order: Tech A and Tech B share only the existing `ir::Module`; Tech B lowers to LLVM and knows nothing about A; Tech C consumes any produced executable and knows nothing about A or B. Each can be removed without breaking the other two.

## Answers to the architectural questions (grounded in the audited code)

1. **What makes BYlink technically different from a conventional LLVM-based language?** Today, only two things are real: all arithmetic and memory behaviour is fully defined (wrapping, explicit traps, no `nsw`, no poison, no uninitialised memory), and the language has an AI-oriented specification. Everything else (speed, optimizations, targets) is LLVM's. The differentiators to build are a LLM-facing protocol with a strict validator and machine-readable diagnostics, a backend-independent semantic specification, and a signed build chain. Nothing here is a performance differentiator until measured.
2. **What stays independent of LLVM?** Lexer/parser/sema, BYlink IR, BPIA and its verifier, the AI protocol and `.byb`, diagnostics, BYX container, licensing. Only text emission in `compiler/pia/llvm_emit.cpp` knows LLVM.
3. **Own IR?** Yes, and there are exactly two levels: BYlink IR (front-end level: slots, signedness in types) and BPIA (low level: SSA, signless integers, explicit memory). More levels are not justified.
4. **Own machine-code backend?** Not now. Measured trigger conditions are in `BYLINK_BACKEND_STRATEGY.md`: cloud compile latency dominated by Clang, reproducibility requirements Clang cannot meet, or a target Clang lacks. A hybrid (LLVM for optimized builds, a small custom baseline backend later for fast builds) is the only custom-backend option with a plausible case.
5. **How can an LLM generate valid programs more efficiently?** Fixed-arity prefix syntax, no punctuation, one statement per line, a single canonical form, and a validator with positioned errors (Tech A). Whether it is more efficient is an empirical question, answered in `AI_INSTRUCTION_ENCODING.md` with real tokenizers.
6. **How to measure token savings?** Real BPE vocabularies (OpenAI cl100k/o200k, Llama 3, Llama 1/2 SentencePiece, legacy Claude) run over a program corpus; script in `bench/tokens/`. Generation validity by an LLM cannot be measured offline; a mutation study measures detection instead.
7. **Can BPIA express advanced low-level programs without a runtime?** Yes by construction: it is a compile-time ISA, programs lower to native code with no VM, no GC and no allocator. v0 implements integers, branches, phi, calls and bounds-checked memory regions. Atomics, vectors, raw pointers and threads are specified as planned and rejected by the verifier today.
8. **Native executables for several CPUs?** LLVM triples plus a feature model (`+avx2`, `+neon`, `+zba`), carried in the BYX container as required features. Cross-target assembly emission was verified on five targets (see `BYLINK_BACKEND_STRATEGY.md`); linking foreign executables needs sysroots and is not done.
9. **Defensible proprietary advantage?** Honest ranking: (a) the AI protocol corpus, validator and diagnostics tuned against real model failures, (b) a feature-aware optimizer if it beats Clang in measurements, (c) the managed build/signing service. The current compiler source alone is small and not defensible by secrecy.
10. **What protection is realistic?** Signed artifacts, license entitlements, tamper-evidence and cloud-held optimizer. Not realistic: preventing disassembly or patching of a binary the attacker runs.
11. **Independent of LLVM:** see 2, plus all of Tech C.
12. **Use LLVM for:** instruction selection, register allocation, scalar and vector optimization, assembly, linking, all five target architectures.
13. **What first?** In order: fix the stack-overflow defect, build the IR verifier (needed by everything), then Tech A (cheapest, measurable now), Tech B memory model, Tech C signature layer.

## Prioritized implementation plan

| Priority | Work | Status in this delivery |
|---|---|---|
| P0 | Depth limit in parser/sema/lower (audit finding 1) | done |
| P0 | `ir::verify` | done |
| P1 | BYAI text + `.byb` + tokenizer study | done (Prototype A) |
| P1 | BPIA v0 + verifier + LLVM lowering; replace direct IR-to-LLVM emitter | in progress |
| P1 | BYX container, Ed25519 signing, offline license | in progress |
| P2 | Multi-diagnostic reporting, machine-readable (JSON) diagnostics | not started |
| P2 | Arrays/pointers in the source language on top of BPIA memory | not started |
| P2 | `--cpu`/feature flags and foreign-triple emission in the CLI | done (see `PORTABILITY_AUDIT.md`) |
| P3 | Vector types, atomics, threads in BPIA | specified, not implemented |
| P3 | Cloud compile service | design only |

## Versioning and compatibility

- Language `0.1`, IR `0` (text and `.byb`), BYAI `1`, BPIA `0`, BYX `0`, license `0`. Each artifact carries its own version on the first line or header.
- Within a major version, new instructions may be added; a consumer rejects unknown opcodes (never ignores them). A major bump is required to change the meaning of an existing instruction.
- The existing `.by` language and all 42 baseline tests are unchanged.

## Migration plan

Planned: `ir::Module` to LLVM passes through BPIA and the old direct emitter is deleted once all baseline tests pass unmodified. Nothing else is replaced.
