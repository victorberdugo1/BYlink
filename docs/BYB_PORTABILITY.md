# BYB Portability

## What is guaranteed

Given the same compiler front end and the same IR, `.byb` output is byte-identical on every host. The writer produces bytes with explicit shifts and masks. It never writes a struct, pointer, enum ordinal, `size_t` or any host-sized value; wire codes come from fixed tables; there is no padding.

## Evidence

| Check | Result |
|---|---|
| Golden bytes derived by hand from the specification (`byb_golden_bytes_are_pinned_by_the_specification`) | pass |
| `encode(decode(x)) == x` for all examples at `-O0 .. -Os` | pass |
| Every truncation of a valid file is rejected; every single-bit corruption is either rejected or re-encodes to exactly the corrupted bytes | pass (corpus of 7 programs) |
| Non-canonical varints, hostile names, bad codes, bad indices, oversized input | rejected |
| `tests/cross_host_byb.sh`: `--emit-byb` SHA-256 for 14 programs at `-O0` and `-O2`, `bylc` built and run on x86-64 (native), AArch64, riscv64 (both little-endian, QEMU user-mode) and s390x (big-endian, QEMU user-mode) | identical, 28 of 28 |

## Limits of the evidence

- Three of the four hosts are emulated. No physical non-x86 machine was used.
- All four builds used GCC 13.3. No MSVC or Apple Clang build was checked.
- Mutation testing is exhaustive for single-bit flips and truncations on a small corpus, not a coverage-guided fuzzing campaign (no libFuzzer or AFL run).
- Windows and macOS hosts were not tested.

## Conditions for the guarantee

The guarantee covers the IR to `.byb` step. If the front end or optimizer changes, the IR changes and so do the bytes; version 0 does not promise stability across compiler versions at a given `-O` level, only that decoding and re-encoding is stable. Use `-O0` or a stored `.byb` as the reference artifact when a fixed byte sequence is needed.
