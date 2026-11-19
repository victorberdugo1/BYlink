# AI Instruction Encoding: representations and measurements

Four representations of the same program were compared. All numbers below were produced by the code in `bench/` on this repository's 14-program corpus (`bench/corpus`, `examples`; 6,232 bytes of source). Reproduce: `node bench/tokens/measure.mjs build/bylc bench/corpus examples` (after `npm install` in `bench/tokens`), and `bylink_ai_bench time|mutate`.

## The representations

| Name | Form | Role |
|---|---|---|
| Source | `.by` | conventional baseline |
| BYAI | `.byai`, see `AI_PROTOCOL.md` | what an LLM writes |
| IR text | `--emit-ir` | structured IR, slots and registers |
| `.byb` | binary IR | tool-to-tool exchange |

### `.byb`

The layout, canonical form, limits and decoder obligations are specified in `BYB_FORMAT_SPECIFICATION.md`; portability evidence is in `BYB_PORTABILITY.md`. Decoding then re-encoding an accepted file reproduces it byte for byte (tested for every accepted single-bit corruption).

## Token counts

Tokenizers are the real vocabularies shipped in npm packages: OpenAI `cl100k_base` and `o200k_base` (gpt-tokenizer 4.1.2), Llama 3 (llama3-tokenizer-js 1.2.0), Llama 1/2 SentencePiece (llama-tokenizer-js 1.2.2) and the legacy Claude BPE (@anthropic-ai/tokenizer 0.0.4). Vocabulary downloads from the model hubs were not reachable from the build sandbox. Caveats: the legacy Claude tokenizer is not the tokenizer of current Claude models; Llama 3 uses a vocabulary that extends `cl100k_base`, so for this ASCII corpus its counts equal cl100k's and are not an independent data point. Model-specific tokenizers of other vendors were not available and were not measured.

Totals over the corpus (change vs source in parentheses):

| Representation | bytes | cl100k | o200k | Llama 3 | Llama 1/2 | Claude legacy |
|---|---|---|---|---|---|---|
| source | 6232 | 2290 | 2294 | 2290 | 2979 | 2364 |
| source, whitespace collapsed | 4960 | 2076 (-9.3%) | 2080 (-9.3%) | 2076 | 2415 (-18.9%) | 2014 (-14.8%) |
| **BYAI** (canonical, flat) | 4722 | 1964 (-14.2%) | 1957 (-14.7%) | 1964 | 2292 (-23.1%) | 1851 (-21.7%) |
| BYAI indented 2 spaces | 5330 | 2178 (-4.9%) | 2171 (-5.4%) | 2178 | 2506 (-15.9%) | 1851 (-21.7%) |
| IR text -O0 | 15797 | 8210 (+258%) | 8217 (+258%) | 8210 | 9742 (+227%) | 7738 (+227%) |
| IR text -O2 | 15774 | 8197 (+258%) | 8204 (+258%) | 8197 | 9726 (+227%) | 7727 (+227%) |
| `.byb` as hex | 8286 | 3259 (+42%) | 3260 (+42%) | 3259 | 8266 (+178%) | 3562 (+51%) |
| `.byb` as base64 | 5544 | 3399 (+48%) | 3265 (+42%) | 3392 | 3920 (+32%) | 3294 (+39%) |

Findings:

1. BYAI is 14-23% cheaper than source depending on the tokenizer. About two thirds of that comes from removing whitespace and newlines (collapsed source already saves 9-19%); the dense prefix syntax contributes roughly 5 percentage points on top. The canonical form therefore carries no indentation (indentation alone costs about 9 points on the BPE tokenizers).
2. IR text is 3.3-3.6 times more expensive than source. The slot-based IR (one load per variable read, one store per write) is not a good generation target.
3. A binary format is not a token-efficient way to talk to an LLM: any text armouring of bytes costs 30-180% more tokens than source. `.byb` is for tools, not for models, which confirms the project's premise that fewer bytes do not mean fewer tokens.

## Speed (same machine, 14 programs, microseconds for one pass over the corpus, text to verified IR)

| Input | bytes | time |
|---|---|---|
| source | 6232 | 278 us |
| BYAI | 4722 | 262 us |
| `.byb` | 4143 | 88 us |

Text parsing is not the bottleneck (BYAI and source are equal within noise, and both include semantic analysis). `.byb` is about 3x faster because it skips parsing and semantic analysis and runs only the verifier. Measured with a single-threaded steady-clock loop of 300 iterations per program on a shared VM: treat as order of magnitude.

## Error resistance (mutation study, not a generation study)

28,000 single-lexeme mutations per representation (delete, duplicate, swap with next, replace with another lexeme from the same file), fixed seeds. "Silent change" = mutated text is accepted and produces different IR, the dangerous case for an LLM repair loop.

| Representation | rejected | accepted, same IR | accepted, different IR |
|---|---|---|---|
| source (seed 1 / seed 7) | 97.6% / 97.6% | 1.2% / 1.2% | **1.19% / 1.21%** |
| BYAI (seed 1 / seed 7) | 87.4% / 87.9% | 8.3% / 8.1% | **4.30% / 4.02%** |

BYAI is **less** resistant than source to this class of damage: its density means more lexemes are operands that can be replaced by another valid operand. (Most "same IR" cases are duplicated newlines, which BYAI ignores.) The mutation study measures how a representation reacts to random damage; it does not measure how often an LLM produces damage. Reducing the silent-change rate would require redundancy (for example typed operators), which costs tokens; that trade-off was not explored.

## What was not measured

- Generation validity, error rate and repair turns of any LLM on any representation. No model was queried; the claim "BYAI is easier for LLMs to generate" is therefore **unproven**. Only the token saving (14-23%) and the decode properties above are measured.
- Tokenizers of current commercial models other than those listed.
- Larger or LLM-written corpora (the corpus is 14 hand-written programs).

## Decisions

- Keep BYAI: a measured 14-23% token saving, one canonical form, 3 validation layers, no cost in compiler complexity (the decoder and printer are about 600 lines and reuse the front end).
- Do not offer IR text as a generation target.
- Keep `.byb` for tooling, caching and the signed distribution format.
- Next experiment: run a fixed prompt set against real models in `source` and `byai` modes and compare validity and repair turns before making any claim about generation quality.
