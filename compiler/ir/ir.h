#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "types/types.h"

namespace bylink::ir {

struct Value {
  bool isConst = false;
  Type type = Type::Void;
  uint32_t reg = 0;
  uint64_t bits = 0;

  static Value constant(Type type, uint64_t bits) { return Value{true, type, 0, truncate(type, bits)}; }
  static Value fromReg(Type type, uint32_t reg) { return Value{false, type, reg, 0}; }
};

enum class Opcode { Binary, Compare, Cast, Load, Store, Call, Br, CondBr, Ret };

struct Instr {
  Opcode op = Opcode::Ret;
  Type type = Type::Void;
  Type fromType = Type::Void;
  BinOp bin = BinOp::Add;
  CmpOp cmp = CmpOp::Eq;
  std::optional<uint32_t> dst;
  std::vector<Value> operands;
  uint32_t slot = 0;
  std::string callee;
  uint32_t target0 = 0;
  uint32_t target1 = 0;
};

struct Block {
  std::vector<Instr> instrs;
};

struct Function {
  std::string name;
  std::vector<Type> params;
  Type ret = Type::Void;
  std::vector<Type> slots;
  std::vector<Block> blocks;
  uint32_t regCount = 0;
};

struct Module {
  std::string name;
  std::vector<Function> functions;

  const Function* find(const std::string& functionName) const;
};

bool isTerminator(Opcode op);
std::string print(const Module& module);

}
