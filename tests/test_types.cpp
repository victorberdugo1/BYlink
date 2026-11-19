#include "harness.h"
#include "types/types.h"

using namespace bylink;

TEST(types_wrapping_arithmetic) {
  CHECK_EQ(*evalBinary(BinOp::Add, Type::U8, 255, 1), 0u);
  CHECK_EQ(*evalBinary(BinOp::Sub, Type::U8, 0, 1), 255u);
  CHECK_EQ(signedValue(Type::I8, *evalBinary(BinOp::Add, Type::I8, 127, 1)), -128);
  CHECK_EQ(*evalBinary(BinOp::Mul, Type::U32, 0x10000, 0x10000), 0u);
}

TEST(types_division_traps) {
  CHECK(!evalBinary(BinOp::Div, Type::I32, 1, 0));
  CHECK(!evalBinary(BinOp::Rem, Type::U32, 1, 0));
  CHECK(!evalBinary(BinOp::Div, Type::I8, truncate(Type::I8, static_cast<uint64_t>(-128)), truncate(Type::I8, static_cast<uint64_t>(-1))));
  CHECK_EQ(signedValue(Type::I32, *evalBinary(BinOp::Div, Type::I32, truncate(Type::I32, static_cast<uint64_t>(-7)), 2)), -3);
  CHECK_EQ(signedValue(Type::I32, *evalBinary(BinOp::Rem, Type::I32, truncate(Type::I32, static_cast<uint64_t>(-7)), 2)), -1);
}

TEST(types_literal_ranges) {
  CHECK(literalFits(Type::I8, true, 128));
  CHECK(!literalFits(Type::I8, false, 128));
  CHECK(literalFits(Type::U8, false, 255));
  CHECK(!literalFits(Type::U8, true, 1));
  CHECK(literalFits(Type::I64, true, uint64_t{1} << 63));
  CHECK(!literalFits(Type::Bool, false, 1));
}

TEST(types_casts) {
  CHECK_EQ(evalCast(Type::I32, Type::U8, 300), 44u);
  CHECK_EQ(evalCast(Type::I32, Type::U8, truncate(Type::I32, static_cast<uint64_t>(-1))), 255u);
  CHECK_EQ(signedValue(Type::I64, evalCast(Type::I8, Type::I64, 0xFF)), -1);
  CHECK_EQ(evalCast(Type::U8, Type::I64, 0xFF), 255u);
  CHECK_EQ(evalCast(Type::Bool, Type::I32, 1), 1u);
}

TEST(types_comparison_signedness) {
  CHECK(evalCompare(CmpOp::Lt, Type::I8, 0xFF, 1));
  CHECK(!evalCompare(CmpOp::Lt, Type::U8, 0xFF, 1));
}
