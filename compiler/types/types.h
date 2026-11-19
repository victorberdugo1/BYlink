#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace bylink {

enum class Type { I8, I16, I32, I64, U8, U16, U32, U64, Bool, Void };
enum class BinOp { Add, Sub, Mul, Div, Rem, And, Or, Xor };
enum class CmpOp { Eq, Ne, Lt, Le, Gt, Ge };

bool isInteger(Type type);
bool isSigned(Type type);
unsigned bitWidth(Type type);
const char* typeName(Type type);
std::optional<Type> typeFromName(std::string_view name);

uint64_t truncate(Type type, uint64_t bits);
int64_t signedValue(Type type, uint64_t bits);
int64_t minSigned(Type type);

bool literalFits(Type type, bool negative, uint64_t magnitude);
uint64_t literalBits(Type type, bool negative, uint64_t magnitude);

std::optional<uint64_t> evalBinary(BinOp op, Type type, uint64_t a, uint64_t b);
bool evalCompare(CmpOp op, Type type, uint64_t a, uint64_t b);
uint64_t evalCast(Type from, Type to, uint64_t bits);

}
