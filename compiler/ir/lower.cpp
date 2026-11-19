#include "ir/lower.h"

#include <stdexcept>

namespace bylink::ir {

namespace {

using ast::BinaryKind;
using ast::Expr;
using ast::ExprKind;
using ast::Stmt;
using ast::StmtKind;
using ast::StmtList;
using ast::UnaryKind;

class FunctionLowerer {
 public:
  explicit FunctionLowerer(const ast::Function& fn) : fn_(fn) {}

  Function run() {
    out_.name = fn_.name;
    out_.ret = fn_.ret;
    for (const ast::Param& p : fn_.params) out_.params.push_back(p.type);
    for (const ast::Local& l : fn_.locals) out_.slots.push_back(l.type);
    out_.regCount = static_cast<uint32_t>(fn_.params.size());
    setBlock(newBlock());
    for (size_t i = 0; i < fn_.params.size(); ++i) {
      store(static_cast<uint32_t>(i), Value::fromReg(fn_.params[i].type, static_cast<uint32_t>(i)));
    }
    lowerBlock(fn_.body);
    if (cur_) {
      if (fn_.ret != Type::Void) throw std::logic_error("lowering reached the end of a non-void function");
      emitRet(std::nullopt);
    }
    return std::move(out_);
  }

 private:
  struct Loop {
    uint32_t exit;
    uint32_t header;
  };

  const ast::Function& fn_;
  Function out_;
  std::optional<uint32_t> cur_;
  std::vector<Loop> loops_;

  uint32_t newBlock() {
    out_.blocks.emplace_back();
    return static_cast<uint32_t>(out_.blocks.size() - 1);
  }

  void setBlock(uint32_t block) { cur_ = block; }

  uint32_t newReg() { return out_.regCount++; }

  uint32_t newSlot(Type type) {
    out_.slots.push_back(type);
    return static_cast<uint32_t>(out_.slots.size() - 1);
  }

  void emit(Instr instr) {
    bool terminator = isTerminator(instr.op);
    out_.blocks[*cur_].instrs.push_back(std::move(instr));
    if (terminator) cur_.reset();
  }

  Value emitWithResult(Instr instr, Type resultType) {
    uint32_t reg = newReg();
    instr.dst = reg;
    emit(std::move(instr));
    return Value::fromReg(resultType, reg);
  }

  Value binary(BinOp op, Type type, Value a, Value b) {
    Instr i;
    i.op = Opcode::Binary;
    i.type = type;
    i.bin = op;
    i.operands = {a, b};
    return emitWithResult(std::move(i), type);
  }

  Value load(uint32_t slot, Type type) {
    Instr i;
    i.op = Opcode::Load;
    i.type = type;
    i.slot = slot;
    return emitWithResult(std::move(i), type);
  }

  void store(uint32_t slot, Value value) {
    Instr i;
    i.op = Opcode::Store;
    i.type = value.type;
    i.slot = slot;
    i.operands = {value};
    emit(std::move(i));
  }

  void br(uint32_t target) {
    Instr i;
    i.op = Opcode::Br;
    i.target0 = target;
    emit(std::move(i));
  }

  void condBr(Value cond, uint32_t ifTrue, uint32_t ifFalse) {
    Instr i;
    i.op = Opcode::CondBr;
    i.operands = {cond};
    i.target0 = ifTrue;
    i.target1 = ifFalse;
    emit(std::move(i));
  }

  void emitRet(std::optional<Value> value) {
    Instr i;
    i.op = Opcode::Ret;
    i.type = fn_.ret;
    if (value) i.operands = {*value};
    emit(std::move(i));
  }

  void lowerBlock(const StmtList& list) {
    for (const auto& stmt : list) {
      if (!cur_) return;
      lowerStmt(*stmt);
    }
  }

  void lowerStmt(const Stmt& s) {
    switch (s.kind) {
      case StmtKind::Let:
      case StmtKind::Assign:
        store(static_cast<uint32_t>(s.local), lowerExpr(*s.expr));
        break;
      case StmtKind::If: lowerIf(s); break;
      case StmtKind::While: lowerWhile(s); break;
      case StmtKind::Return:
        emitRet(s.expr ? std::optional<Value>(lowerExpr(*s.expr)) : std::nullopt);
        break;
      case StmtKind::Break: br(loops_.back().exit); break;
      case StmtKind::Continue: br(loops_.back().header); break;
      case StmtKind::Call: lowerExpr(*s.expr); break;
      case StmtKind::Block: lowerBlock(s.body); break;
    }
  }

  void lowerIf(const Stmt& s) {
    Value cond = lowerExpr(*s.expr);
    std::optional<uint32_t> merge;
    auto mergeBlock = [&] {
      if (!merge) merge = newBlock();
      return *merge;
    };
    uint32_t thenBlock = newBlock();
    std::optional<uint32_t> elseBlock;
    if (s.hasElse) elseBlock = newBlock();
    condBr(cond, thenBlock, elseBlock ? *elseBlock : mergeBlock());
    setBlock(thenBlock);
    lowerBlock(s.body);
    if (cur_) br(mergeBlock());
    if (elseBlock) {
      setBlock(*elseBlock);
      lowerBlock(s.elseBody);
      if (cur_) br(mergeBlock());
    }
    if (merge) setBlock(*merge);
  }

  void lowerWhile(const Stmt& s) {
    uint32_t header = newBlock();
    uint32_t body = newBlock();
    uint32_t exit = newBlock();
    br(header);
    setBlock(header);
    condBr(lowerExpr(*s.expr), body, exit);
    setBlock(body);
    loops_.push_back({exit, header});
    lowerBlock(s.body);
    loops_.pop_back();
    if (cur_) br(header);
    setBlock(exit);
  }

  Value lowerExpr(const Expr& e) {
    if (e.constBits) return Value::constant(e.type, *e.constBits);
    switch (e.kind) {
      case ExprKind::Ident: return load(static_cast<uint32_t>(e.local), e.type);
      case ExprKind::Unary: return lowerUnary(e);
      case ExprKind::Binary: return lowerBinary(e);
      case ExprKind::Call: return lowerCall(e);
      case ExprKind::Cast: return lowerCast(e);
      default: throw std::logic_error("literal without a constant value");
    }
  }

  Value lowerUnary(const Expr& e) {
    Value operand = lowerExpr(*e.operands[0]);
    switch (e.unary) {
      case UnaryKind::Neg: return binary(BinOp::Sub, e.type, Value::constant(e.type, 0), operand);
      case UnaryKind::Not: return binary(BinOp::Xor, Type::Bool, operand, Value::constant(Type::Bool, 1));
      case UnaryKind::BitNot: return binary(BinOp::Xor, e.type, operand, Value::constant(e.type, ~uint64_t{0}));
    }
    throw std::logic_error("unknown unary operator");
  }

  Value lowerBinary(const Expr& e) {
    if (ast::isLogical(e.binary)) return lowerLogical(e);
    Value a = lowerExpr(*e.operands[0]);
    Value b = lowerExpr(*e.operands[1]);
    if (!ast::isComparison(e.binary)) return binary(ast::toBinOp(e.binary), e.type, a, b);
    Instr i;
    i.op = Opcode::Compare;
    i.type = e.operands[0]->type;
    i.cmp = ast::toCmpOp(e.binary);
    i.operands = {a, b};
    return emitWithResult(std::move(i), Type::Bool);
  }

  Value lowerLogical(const Expr& e) {
    bool isAnd = e.binary == BinaryKind::LogAnd;
    uint32_t slot = newSlot(Type::Bool);
    Value lhs = lowerExpr(*e.operands[0]);
    store(slot, lhs);
    uint32_t rhsBlock = newBlock();
    uint32_t endBlock = newBlock();
    condBr(lhs, isAnd ? rhsBlock : endBlock, isAnd ? endBlock : rhsBlock);
    setBlock(rhsBlock);
    store(slot, lowerExpr(*e.operands[1]));
    br(endBlock);
    setBlock(endBlock);
    return load(slot, Type::Bool);
  }

  Value lowerCall(const Expr& e) {
    Instr i;
    i.op = Opcode::Call;
    i.type = e.type;
    i.callee = e.name;
    for (const auto& arg : e.operands) i.operands.push_back(lowerExpr(*arg));
    if (e.type == Type::Void) {
      emit(std::move(i));
      return Value{};
    }
    return emitWithResult(std::move(i), e.type);
  }

  Value lowerCast(const Expr& e) {
    Value operand = lowerExpr(*e.operands[0]);
    Instr i;
    i.op = Opcode::Cast;
    i.type = e.castTo;
    i.fromType = operand.type;
    i.operands = {operand};
    return emitWithResult(std::move(i), e.castTo);
  }
};

}

Module lower(const ast::Module& module) {
  Module out;
  out.name = module.name;
  for (const ast::Function& f : module.functions) out.functions.push_back(FunctionLowerer(f).run());
  return out;
}

}
