# BYB Format Specification, version 0

Status: **Implemented** (`compiler/ai/byb.cpp`). `.byb` is the binary serialization of target-independent BYlink IR. It contains no machine code and no target information.

## Conventions

- Byte order: the format is a byte stream. Every multi-byte quantity is an unsigned LEB128 varint, so byte order of the host or the target never applies.
- `varint`: unsigned LEB128, 1 to 10 bytes, **shortest form only**. A final byte of `0x00` after a continuation, an 11th byte, or a 10th byte greater than `0x01` is an error. Values are at most 64 bits.
- `index32`: a varint that must not exceed 2^32 - 1.
- `count`: a varint that must not exceed the number of bytes remaining in the file (every element takes at least one byte). This bounds allocation by the input size.
- `string`: `varint length` followed by that many bytes. Names are ASCII identifiers (see Limits); no other encoding is defined or accepted.
- `zigzag(v)`: `(v << 1) ^ (v >> 63)` on a 64-bit signed value.

## Grammar

```
file      = "BYIR" version module
version   = 0x00
module    = string(name) count(functions) function*
function  = string(name)
            count(params) type*
            type(return)
            count(slots) type*
            index32(registerCount)
            count(blocks) block*
block     = count(instructions) instruction*
```

Parameters occupy registers `0 .. params-1`. Slots `0 .. slots-1` are typed stack cells. Block 0 is the entry block.

### Type codes (one byte)

| Code | Type | Code | Type |
|---|---|---|---|
| 0 | i8 | 5 | u16 |
| 1 | i16 | 6 | u32 |
| 2 | i32 | 7 | u64 |
| 3 | i64 | 8 | bool |
| 4 | u8 | 9 | void |

### Operand (`value`)

One tag byte `(typeCode << 1) | isConstant`, then:
- register (`isConstant = 0`): `index32` register number;
- constant of a signed type: `varint(zigzag(signed value))`;
- constant of an unsigned type or `bool`: `varint(value)`.

A constant must fit its type exactly; otherwise it is rejected (this also keeps the encoding canonical).

### Instructions

Each starts with an opcode byte.

| Code | Opcode | Fields |
|---|---|---|
| 0 | Binary | `type`, `binop` byte, `index32 dst`, `value`, `value` |
| 1 | Compare | `type` (operand type), `cmp` byte, `index32 dst`, `value`, `value` |
| 2 | Cast | `type from`, `type to`, `index32 dst`, `value` |
| 3 | Load | `type`, `index32 dst`, `index32 slot` |
| 4 | Store | `type`, `index32 slot`, `value` |
| 5 | Call | `type` (return), `index32 callee` (function table index), `index32 dst` only if the type is not void, `count(arguments)`, `value*` |
| 6 | Br | `index32 target block` |
| 7 | CondBr | `value` (bool), `index32 true block`, `index32 false block` |
| 8 | Ret | `type`, `value` only if the type is not void |

Binary operation codes: 0 Add, 1 Sub, 2 Mul, 3 Div, 4 Rem, 5 And, 6 Or, 7 Xor. Comparison codes: 0 Eq, 1 Ne, 2 Lt, 3 Le, 4 Gt, 5 Ge. Signedness of a comparison or division comes from the type.

These codes are fixed by explicit tables in the implementation, not by the order of C++ enumerators.

## Canonical form

For a given IR module there is exactly one encoding: shortest varints, one representation per constant, functions and instructions in IR order, calls by table index, no padding, no optional fields. The encoder calls the verifier first and refuses to write invalid IR.

## Decoder obligations

The decoder rejects, with a diagnostic and without undefined behaviour: files over 64 MiB, bad magic, any version other than 0, truncated data, trailing bytes, non-minimal or overlong varints, counts larger than the remaining data, indices over 2^32 - 1, unknown type, opcode, binary-operation or comparison codes, constants that do not fit their type, calls to a function index that does not exist, and any module that fails `ir::verify` (invalid names, undefined or duplicate registers, type mismatches, bad branch targets, a block without a terminator, use not dominated by its definition, more than 16384 blocks per function, more than 2^24 registers).

## Limits

| Item | Limit |
|---|---|
| File size | 64 MiB |
| Name | ASCII `[A-Za-z_][A-Za-z0-9_]*`, 1 to 1024 bytes |
| Blocks per function | 16384 |
| Registers per function | 2^24 |

## Versioning and migration

Only version 0 exists. The decoder rejects every other version. Any change to the layout, including adding a field, requires a new version number. Version 0 has **no** section table, extension mechanism, checksum or signature. Those are **Designed, not implemented**; adding them means defining version 1 and a migration path. A signature or a target-specific container, if ever added, must wrap or follow this IR, not alter it.
