# AI-Native Instruction Encoding: architecture overview

Detailed specifications: `AI_PROTOCOL.md` (BYAI grammar and validation), `AI_INSTRUCTION_ENCODING.md` (representations, `.byb`, measurements), `AI_GENERATION_GUIDE.md` (prompt for models).

## Design

```
 LLM output (.byai) --decode--> AST --sema--> BYlink IR --verify--> optimizer --> backend
 .by source ---------parse----> AST ^                   ^
 .byb (binary IR) ------decode + verify-----------------+
```

BYAI is a re-spelling of the existing language, not a new one. That choice has three consequences: semantics cannot diverge (one checker), compiler complexity grows by one decoder, and the entire existing test suite remains the semantic oracle. Equivalence is tested as IR identity: for every corpus program and optimization level, `source`, `byai` and `byb` produce identical IR text.

## Why not generate IR or bytes

Measured with real tokenizers: IR text costs 3.3-3.6x the tokens of source (every variable access is a load/store pair), and a binary format costs 30-180% more tokens than source once armoured as text. A compact byte count says nothing about token count; the design therefore optimizes the text surface (no punctuation, no indentation, fixed-arity prefix operators) and uses binary only between tools.

## Measured result and its limit

BYAI saves 14-23% tokens versus source on five real tokenizers; most of that is whitespace. Its error resistance under random token damage is lower than source (4.0-4.3% silent semantic change versus 1.2%). Whether LLMs generate valid BYAI more reliably than source has **not** been measured, because no model could be queried here; this is the first experiment to run (see `AI_INSTRUCTION_ENCODING.md`).

## Components

| Component | Visibility | Files |
|---|---|---|
| BYAI protocol and guide | public | `docs/AI_*.md` |
| BYAI decoder/encoder, `.byb` codec, IR verifier | compiler (proprietary) | `compiler/ai/*`, `compiler/ir/verify.*` |
| Token and mutation study | public methodology | `bench/tokens`, `bench/ai_bench.cpp` |
