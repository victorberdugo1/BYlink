#include "types/types.h"

#include <array>
#include <limits>

namespace bylink {

namespace {

struct TypeInfo {
  Type type;
  const char* name;
  unsigned width;
  bool isSigned;
  bool isInteger;
};

constexpr std::array<TypeInfo, 10> kTypes{{
    {Type::I8, "i8", 8, true, true},
    {Type::I16, "i16", 16, true, true},
    {Type::I32, "i32", 32, true, true},
    {Type::I64, "i64", 64, true, true},
    {Type::U8, "u8", 8, false, true},
    {Type::U16, "u16", 16, false, true},
    {Type::U32, "u32", 32, false, true},
    {Type::U64, "u64", 64, false, true},
    {Type::Bool, "bool", 1, false, false},
    {Type::Void, "void", 0, false, false},
}};

const TypeInfo& info(Type type) {
  for (const TypeInfo& entry : kTypes) {
    if (entry.type == type) return entry;
  }
  return kTypes.back();
}

uint64_t mask(Type type) {
  unsigned width = bitWidth(type);
  return width >= 64 ? ~uint64_t{0} : (uint64_t{1} << width) - 1;
}

}

bool isInteger(Type type) { return info(type).isInteger; }
bool isSigned(Type type) { return info(type).isSigned; }
unsigned bitWidth(Type type) { return info(type).width; }
const char* typeName(Type type) { return info(type).name; }

std::optional<Type> typeFromName(std::string_view name) {
  for (const TypeInfo& entry : kTypes) {
    if (name == entry.name) return entry.type;
  }
  return std::nullopt;
}

uint64_t truncate(Type type, uint64_t bits) { return bits & mask(type); }

int64_t signedValue(Type type, uint64_t bits) {
  unsigned width = bitWidth(type);
  uint64_t value = truncate(type, bits);
  if (width < 64 && ((value >> (width - 1)) & 1) != 0) value |= ~mask(type);
  return static_cast<int64_t>(value);
}

int64_t minSigned(Type type) {
  unsigned width = bitWidth(type);
  if (width == 64) return std::numeric_limits<int64_t>::min();
  return -(int64_t{1} << (width - 1));
}

bool literalFits(Type type, bool negative, uint64_t magnitude) {
  if (!isInteger(type)) return false;
  unsigned width = bitWidth(type);
  if (isSigned(type)) {
    uint64_t limit = uint64_t{1} << (width - 1);
    return negative ? magnitude <= limit : magnitude < limit;
  }
  if (negative) return magnitude == 0;
  return magnitude <= mask(type);
}

uint64_t literalBits(Type type, bool negative, uint64_t magnitude) {
  return truncate(type, negative ? uint64_t{0} - magnitude : magnitude);
}

std::optional<uint64_t> evalBinary(BinOp op, Type type, uint64_t a, uint64_t b) {
  a = truncate(type, a);
  b = truncate(type, b);
  switch (op) {
    case BinOp::Add: return truncate(type, a + b);
    case BinOp::Sub: return truncate(type, a - b);
    case BinOp::Mul: return truncate(type, a * b);
    case BinOp::And: return a & b;
    case BinOp::Or: return a | b;
    case BinOp::Xor: return a ^ b;
    case BinOp::Div:
    case BinOp::Rem: {
      if (b == 0) return std::nullopt;
      if (!isSigned(type)) return op == BinOp::Div ? a / b : a % b;
      int64_t x = signedValue(type, a);
      int64_t y = signedValue(type, b);
      if (y == -1 && x == minSigned(type)) return std::nullopt;
      return truncate(type, static_cast<uint64_t>(op == BinOp::Div ? x / y : x % y));
    }
  }
  return std::nullopt;
}

bool evalCompare(CmpOp op, Type type, uint64_t a, uint64_t b) {
  if (isSigned(type)) {
    int64_t x = signedValue(type, a);
    int64_t y = signedValue(type, b);
    switch (op) {
      case CmpOp::Eq: return x == y;
      case CmpOp::Ne: return x != y;
      case CmpOp::Lt: return x < y;
      case CmpOp::Le: return x <= y;
      case CmpOp::Gt: return x > y;
      case CmpOp::Ge: return x >= y;
    }
  }
  uint64_t x = truncate(type, a);
  uint64_t y = truncate(type, b);
  switch (op) {
    case CmpOp::Eq: return x == y;
    case CmpOp::Ne: return x != y;
    case CmpOp::Lt: return x < y;
    case CmpOp::Le: return x <= y;
    case CmpOp::Gt: return x > y;
    case CmpOp::Ge: return x >= y;
  }
  return false;
}

uint64_t evalCast(Type from, Type to, uint64_t bits) {
  uint64_t extended = isSigned(from) ? static_cast<uint64_t>(signedValue(from, bits)) : truncate(from, bits);
  return truncate(to, extended);
}

}
