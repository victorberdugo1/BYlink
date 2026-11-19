#include "ir/verify.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "common/diagnostic.h"

namespace bylink::ir {

namespace {

constexpr size_t kMaxBlocks = 16384;
constexpr size_t kMaxNameLength = 1024;
constexpr uint32_t kMaxRegisters = uint32_t{1} << 24;

bool isIdentifier(const std::string& name) {
  if (name.empty() || name.size() > kMaxNameLength) return false;
  for (size_t i = 0; i < name.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(name[i]);
    bool alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    bool digit = c >= '0' && c <= '9';
    if (!alpha && !(digit && i != 0)) return false;
  }
  return true;
}

class FunctionVerifier {
 public:
  FunctionVerifier(const Module& module, const Function& fn) : module_(module), fn_(fn) {}

  void run() {
    if (fn_.blocks.empty()) fail("function has no blocks");
    if (fn_.blocks.size() > kMaxBlocks) fail("function has more than " + std::to_string(kMaxBlocks) + " blocks");
    for (Type p : fn_.params) {
      if (!isValueType(p)) fail("parameter has a non-value type");
    }
    for (Type s : fn_.slots) {
      if (!isValueType(s)) fail("slot has a non-value type");
    }
    if (fn_.regCount < fn_.params.size()) fail("register count is smaller than the parameter count");
    if (fn_.regCount > kMaxRegisters) fail("register count exceeds " + std::to_string(kMaxRegisters));
    defineRegisters();
    computeDominators();
    for (size_t b = 0; b < fn_.blocks.size(); ++b) checkBlock(b);
  }

 private:
  struct Def {
    Type type;
    size_t block;
    size_t index;
    bool isParam;
  };

  const Module& module_;
  const Function& fn_;
  std::unordered_map<uint32_t, Def> defs_;
  std::vector<std::vector<bool>> dominators_;

  [[noreturn]] void fail(const std::string& message, int block = -1) const {
    std::string where = "in function '" + fn_.name + "'";
    if (block >= 0) where += ", block bb" + std::to_string(block);
    throw CompileError(Category::Semantic, Loc{}, "invalid IR " + where + ": " + message);
  }

  static bool isValueType(Type t) { return t != Type::Void; }

  static std::optional<Type> resultType(const Instr& in) {
    switch (in.op) {
      case Opcode::Compare: return Type::Bool;
      case Opcode::Binary:
      case Opcode::Cast:
      case Opcode::Load: return in.type;
      case Opcode::Call: return in.type == Type::Void ? std::nullopt : std::optional<Type>(in.type);
      default: return std::nullopt;
    }
  }

  void defineRegisters() {
    for (size_t i = 0; i < fn_.params.size(); ++i) defs_[static_cast<uint32_t>(i)] = {fn_.params[i], 0, 0, true};
    for (size_t b = 0; b < fn_.blocks.size(); ++b) {
      const auto& instrs = fn_.blocks[b].instrs;
      if (instrs.empty()) fail("empty block", static_cast<int>(b));
      for (size_t i = 0; i < instrs.size(); ++i) {
        const Instr& in = instrs[i];
        bool last = i + 1 == instrs.size();
        if (isTerminator(in.op) != last) fail(last ? "block does not end with a terminator" : "terminator in the middle of a block", static_cast<int>(b));
        auto type = resultType(in);
        if (in.dst.has_value() != type.has_value()) fail("instruction result does not match its opcode", static_cast<int>(b));
        if (!in.dst) continue;
        if (*in.dst >= fn_.regCount) fail("register %" + std::to_string(*in.dst) + " is out of range", static_cast<int>(b));
        if (!defs_.emplace(*in.dst, Def{*type, b, i, false}).second) fail("register %" + std::to_string(*in.dst) + " is defined twice", static_cast<int>(b));
      }
    }
  }

  static std::vector<uint32_t> successors(const Instr& term) {
    if (term.op == Opcode::Br) return {term.target0};
    if (term.op == Opcode::CondBr) return {term.target0, term.target1};
    return {};
  }

  void computeDominators() {
    size_t n = fn_.blocks.size();
    std::vector<std::vector<uint32_t>> preds(n);
    for (size_t b = 0; b < n; ++b) {
      for (uint32_t s : successors(fn_.blocks[b].instrs.back())) {
        if (s >= n) fail("branch target bb" + std::to_string(s) + " does not exist", static_cast<int>(b));
        preds[s].push_back(static_cast<uint32_t>(b));
      }
    }
    dominators_.assign(n, std::vector<bool>(n, true));
    dominators_[0].assign(n, false);
    dominators_[0][0] = true;
    for (bool changed = true; changed;) {
      changed = false;
      for (size_t b = 1; b < n; ++b) {
        std::vector<bool> next(n, true);
        for (uint32_t p : preds[b]) {
          for (size_t i = 0; i < n; ++i) next[i] = next[i] && dominators_[p][i];
        }
        next[b] = true;
        if (next != dominators_[b]) {
          dominators_[b] = std::move(next);
          changed = true;
        }
      }
    }
  }

  void checkOperand(const Value& v, size_t block, size_t index, const char* what) {
    if (v.isConst) {
      if (v.type == Type::Void) fail(std::string(what) + " has a void constant", static_cast<int>(block));
      if (v.bits != truncate(v.type, v.bits)) fail(std::string(what) + " has a constant that does not fit its type", static_cast<int>(block));
      return;
    }
    auto it = defs_.find(v.reg);
    if (it == defs_.end()) fail("use of undefined register %" + std::to_string(v.reg), static_cast<int>(block));
    if (it->second.type != v.type) fail("register %" + std::to_string(v.reg) + " used with the wrong type", static_cast<int>(block));
    bool sameBlock = it->second.block == block;
    bool dominates = it->second.isParam || (sameBlock ? it->second.index < index : dominators_[block][it->second.block]);
    if (!dominates) fail("register %" + std::to_string(v.reg) + " is used where its definition does not dominate", static_cast<int>(block));
  }

  void expectOperands(const Instr& in, size_t count, size_t block) {
    if (in.operands.size() != count) fail("wrong operand count", static_cast<int>(block));
  }

  void expectType(const Value& v, Type type, size_t block, const char* what) {
    if (v.type != type) fail(std::string(what) + " has the wrong type", static_cast<int>(block));
  }

  void checkBlock(size_t b) {
    const auto& instrs = fn_.blocks[b].instrs;
    for (size_t i = 0; i < instrs.size(); ++i) {
      const Instr& in = instrs[i];
      for (const Value& v : in.operands) checkOperand(v, b, i, "operand");
      checkInstr(in, b);
    }
  }

  void checkInstr(const Instr& in, size_t b) {
    int blk = static_cast<int>(b);
    switch (in.op) {
      case Opcode::Binary:
        expectOperands(in, 2, b);
        if (!isValueType(in.type)) fail("binary operation on void", blk);
        if (in.type == Type::Bool && in.bin != BinOp::And && in.bin != BinOp::Or && in.bin != BinOp::Xor) fail("arithmetic on bool", blk);
        expectType(in.operands[0], in.type, b, "left operand");
        expectType(in.operands[1], in.type, b, "right operand");
        break;
      case Opcode::Compare:
        expectOperands(in, 2, b);
        if (!isValueType(in.type)) fail("comparison of void", blk);
        if (in.type == Type::Bool && in.cmp != CmpOp::Eq && in.cmp != CmpOp::Ne) fail("ordering comparison of bool", blk);
        expectType(in.operands[0], in.type, b, "left operand");
        expectType(in.operands[1], in.type, b, "right operand");
        break;
      case Opcode::Cast:
        expectOperands(in, 1, b);
        expectType(in.operands[0], in.fromType, b, "cast operand");
        if (!isInteger(in.type)) fail("cast target must be an integer type", blk);
        if (!isInteger(in.fromType) && in.fromType != Type::Bool) fail("cast source must be an integer or bool", blk);
        break;
      case Opcode::Load:
        expectOperands(in, 0, b);
        if (in.slot >= fn_.slots.size()) fail("slot $" + std::to_string(in.slot) + " does not exist", blk);
        if (fn_.slots[in.slot] != in.type) fail("load type differs from the slot type", blk);
        break;
      case Opcode::Store:
        expectOperands(in, 1, b);
        if (in.slot >= fn_.slots.size()) fail("slot $" + std::to_string(in.slot) + " does not exist", blk);
        if (fn_.slots[in.slot] != in.type) fail("store type differs from the slot type", blk);
        expectType(in.operands[0], in.type, b, "stored value");
        break;
      case Opcode::Call: checkCall(in, b); break;
      case Opcode::Br:
        expectOperands(in, 0, b);
        break;
      case Opcode::CondBr:
        expectOperands(in, 1, b);
        expectType(in.operands[0], Type::Bool, b, "branch condition");
        break;
      case Opcode::Ret:
        if (in.type != fn_.ret) fail("return type differs from the function type", blk);
        expectOperands(in, fn_.ret == Type::Void ? 0 : 1, b);
        if (fn_.ret != Type::Void) expectType(in.operands[0], fn_.ret, b, "returned value");
        break;
    }
  }

  void checkCall(const Instr& in, size_t b) {
    int blk = static_cast<int>(b);
    const Function* callee = module_.find(in.callee);
    if (!callee) fail("call to unknown function '" + in.callee + "'", blk);
    if (callee->params.size() != in.operands.size()) fail("call to '" + in.callee + "' has the wrong argument count", blk);
    if (callee->ret != in.type) fail("call to '" + in.callee + "' has the wrong return type", blk);
    for (size_t i = 0; i < in.operands.size(); ++i) expectType(in.operands[i], callee->params[i], b, "call argument");
  }
};

}

void verify(const Module& module) {
  if (!isIdentifier(module.name)) throw CompileError(Category::Semantic, Loc{}, "invalid IR: module name is not an identifier");
  std::unordered_set<std::string> names;
  for (const Function& fn : module.functions) {
    if (!isIdentifier(fn.name)) throw CompileError(Category::Semantic, Loc{}, "invalid IR: function name is not an identifier");
    if (!names.insert(fn.name).second) throw CompileError(Category::Semantic, Loc{}, "invalid IR: duplicate function '" + fn.name + "'");
  }
  for (const Function& fn : module.functions) FunctionVerifier(module, fn).run();
}

}
