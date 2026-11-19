# Target Configuration

Status: **Implemented** unless noted.

## Options

| Option | Meaning |
|---|---|
| `--target NAME` | registry name (`bylc --list-targets`); default `host` |
| `--cpu NAME` | LLVM CPU name, written as the `target-cpu` function attribute; characters `A-Za-z0-9_.-` only |
| `--features LIST` | comma-separated, each `+name` or `-name`, written as `target-features` |
| `--sysroot DIR` | passed to Clang as `--sysroot=DIR` |
| `--linker TOOL` | passed to Clang as `--ld-path=TOOL` (name in `PATH`, or path); only used when linking |
| `--clang PATH` | Clang executable, default `clang` |
| `--list-targets` | one line per target with its verification level |
| `--print-target-info` | all configuration fields for the given options |

Not implemented: `--sdk`. No SDK-dependent target can be linked yet, so the option would do nothing.

## Validation

- Unknown target: `unsupported target 'X'`, exit code 2, no output file.
- CPU and feature strings are checked syntactically before they reach any command line or IR text.
- LLVM ignores an unrecognized CPU or feature with a warning and still exits 0, even under `-Werror`. `bylc` treats those backend messages as errors, removes the output and reports them.
- Missing Clang, missing linker and linker failures give `error[toolchain]` with Clang's own output, exit code 2, and no output file.

## Immutability

`makeTargetConfig` builds a `TargetConfig` once per compilation from `BuildOptions`. The emitter and the toolchain read it and never modify it.

## Example

```
$ bylc --print-target-info --target aarch64-linux-gnu --cpu cortex-a76
name:             aarch64-linux-gnu
llvm triple:      aarch64-unknown-linux-gnu
architecture:     aarch64
operating system: linux
abi:              gnu
object format:    ELF
pointer width:    64
endianness:       little
verified level:   executed under emulation
cpu:              cortex-a76
...
```
