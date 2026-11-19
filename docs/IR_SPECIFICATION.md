# BYlink IR Specification, version 0 (`.byir`)

The IR is produced after semantic analysis and is printable (`bylc --emit-ir`) but the text form is not parseable. Its binary encoding is `.byb`, specified in `BYB_FORMAT_SPECIFICATION.md`.

The IR is target independent: integer types of fixed width, virtual registers, numbered slots, no pointers and no physical registers. Function and module names are ASCII identifiers; the verifier enforces this. Details of what varies by target are in `PORTABILITY_ARCHITECTURE.md`.

## Structure

A module contains functions. A function has typed parameters, a return type, a list of typed slots, and a list of basic blocks numbered `bb0..`. `bb0` is the entry block. Every block ends with exactly one terminator (`br`, `condbr`, `ret`) and contains no other terminator.

Parameters occupy registers `%0..%(n-1)`. The entry block stores each parameter into slot `$i` (slots `0..n-1` are the parameters, followed by locals, followed by compiler temporaries).

Registers are defined exactly once. Mutable state lives only in slots, accessed with `load` and `store`; there are no phi nodes. LLVM promotes slots to registers.

## Text format

```
byir 0
module NAME

func @NAME(T %0, T %1) -> R {
  slot $0: T
bb0:
  store T %0, $0
  %2 = load T $0
  %3 = add T %2, 5
  condbr %4, bb1, bb2
bb1:
  ret T %3
}
```

Constants print as plain decimal (signed types signed, unsigned types unsigned) or `true`/`false`. Types: `i8 i16 i32 i64 u8 u16 u32 u64 bool void`. Signedness is carried by the type and selects signed or unsigned division, remainder, comparison and extension.

## Optimization passes (run at -O1 and above)

1. Constant folding of `add sub mul div rem and or xor`, `cmp` and `cast` with constant operands, using the same arithmetic routines as the compile-time constant evaluator. Trapping operations (division by zero, MIN / -1) are never folded.
2. `condbr` with a constant condition becomes `br`.
3. Unreachable block removal.

The passes repeat until nothing changes. At `-O0` none of them run. Further optimization is performed by Clang/LLVM at the chosen level.

## Lowering to LLVM IR

- Types map to `iN` (`bool` is `i1`); signedness only selects instructions.
- `main` is emitted with external linkage as `@main`; every other function is `internal` and named `@"module.name"`.
- Slots become `alloca` instructions in the entry block.
- `div`/`rem` are emitted with an explicit check that branches to `llvm.trap` on zero divisor and, for signed types, on MIN / -1.
- Same-width casts emit no instruction.
- `target triple` is the output of `clang -dumpmachine`.
