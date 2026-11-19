#include "semantic/sema.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace bylink {

namespace {

using ast::BinaryKind;
using ast::Expr;
using ast::ExprKind;
using ast::Stmt;
using ast::StmtKind;
using ast::StmtList;
using ast::UnaryKind;

struct Signature {
  std::vector<Type> params;
  Type ret;
};

struct ConstInfo {
  Type type;
  uint64_t bits;
};

bool isUntypedLiteral(const Expr& e) { return e.kind == ExprKind::IntLit && !e.suffix; }

class Analyzer {
 public:
  explicit Analyzer(ast::Module& module) : module_(module) {}

  void run() {
    declareGlobals();
    for (ast::Const& c : module_.consts) checkConst(c);
    for (ast::Function& f : module_.functions) checkFunction(f);
  }

 private:
  ast::Module& module_;
  std::unordered_set<std::string> globals_;
  std::unordered_map<std::string, Signature> functions_;
  std::unordered_map<std::string, ConstInfo> consts_;
  ast::Function* fn_ = nullptr;
  std::vector<std::unordered_map<std::string, int>> scopes_;
  int loopDepth_ = 0;

  [[noreturn]] static void fail(Category category, Loc loc, const std::string& message) {
    throw CompileError(category, loc, message);
  }

  static std::string quoted(const std::string& s) { return "'" + s + "'"; }

  static void expectType(Type found, Type expected, Loc loc) {
    if (found != expected) {
      fail(Category::Type, loc,
           std::string("expected ") + quoted(typeName(expected)) + " but found " + quoted(typeName(found)));
    }
  }

  void declareGlobal(const std::string& name, Loc loc) {
    if (!globals_.insert(name).second) fail(Category::Semantic, loc, quoted(name) + " is already defined");
  }

  void declareGlobals() {
    for (const ast::Const& c : module_.consts) declareGlobal(c.name, c.loc);
    for (const ast::Function& f : module_.functions) {
      declareGlobal(f.name, f.loc);
      if (f.name == "main" && (!f.params.empty() || f.ret != Type::I32)) {
        fail(Category::Semantic, f.loc, "'main' must have the signature 'fn main() -> i32'");
      }
      Signature sig{{}, f.ret};
      for (const ast::Param& p : f.params) sig.params.push_back(p.type);
      functions_.emplace(f.name, std::move(sig));
    }
  }

  void checkConst(ast::Const& c) {
    Type found = checkValue(*c.init, c.type);
    expectType(found, c.type, c.init->loc);
    c.bits = evalConst(*c.init);
    consts_.emplace(c.name, ConstInfo{c.type, c.bits});
  }

  uint64_t evalConst(const Expr& e) {
    if (e.constBits) return *e.constBits;
    switch (e.kind) {
      case ExprKind::Unary: {
        uint64_t v = evalConst(*e.operands[0]);
        Type t = e.type;
        switch (e.unary) {
          case UnaryKind::Neg: return *evalBinary(BinOp::Sub, t, 0, v);
          case UnaryKind::Not: return v ^ 1;
          case UnaryKind::BitNot: return truncate(t, ~v);
        }
        break;
      }
      case ExprKind::Binary: {
        uint64_t a = evalConst(*e.operands[0]);
        uint64_t b = evalConst(*e.operands[1]);
        if (e.binary == BinaryKind::LogAnd) return a & b;
        if (e.binary == BinaryKind::LogOr) return a | b;
        Type operandType = e.operands[0]->type;
        if (ast::isComparison(e.binary)) return evalCompare(ast::toCmpOp(e.binary), operandType, a, b) ? 1 : 0;
        auto result = evalBinary(ast::toBinOp(e.binary), e.type, a, b);
        if (!result) fail(Category::Semantic, e.loc, "division by zero or overflow in constant expression");
        return *result;
      }
      case ExprKind::Cast: return evalCast(e.operands[0]->type, e.type, evalConst(*e.operands[0]));
      default: break;
    }
    fail(Category::Semantic, e.loc, "expression is not a compile-time constant");
  }

  void checkFunction(ast::Function& f) {
    fn_ = &f;
    scopes_.clear();
    loopDepth_ = 0;
    scopes_.emplace_back();
    for (const ast::Param& p : f.params) declareLocal(p.name, p.type, false, p.loc);
    checkBlock(f.body);
    if (f.ret != Type::Void && !ast::terminates(f.body)) {
      fail(Category::Semantic, f.loc, "function " + quoted(f.name) + " does not return a value on every path");
    }
    fn_ = nullptr;
  }

  int declareLocal(const std::string& name, Type type, bool isMutable, Loc loc) {
    if (scopes_.back().count(name) != 0) fail(Category::Semantic, loc, quoted(name) + " is already declared in this scope");
    int index = static_cast<int>(fn_->locals.size());
    fn_->locals.push_back(ast::Local{name, type, isMutable});
    scopes_.back()[name] = index;
    return index;
  }

  std::optional<int> findLocal(const std::string& name) const {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
      auto it = scope->find(name);
      if (it != scope->end()) return it->second;
    }
    return std::nullopt;
  }

  void checkBlock(StmtList& list) {
    scopes_.emplace_back();
    for (size_t i = 0; i < list.size(); ++i) {
      if (i > 0 && ast::terminates(*list[i - 1])) fail(Category::Semantic, list[i]->loc, "unreachable statement");
      checkStmt(*list[i]);
    }
    scopes_.pop_back();
  }

  void checkStmt(Stmt& s) {
    switch (s.kind) {
      case StmtKind::Let: {
        Type found = checkValue(*s.expr, s.declType);
        expectType(found, s.declType, s.expr->loc);
        s.local = declareLocal(s.name, s.declType, s.isMutable, s.loc);
        break;
      }
      case StmtKind::Assign: {
        auto local = findLocal(s.name);
        if (!local) {
          if (consts_.count(s.name) != 0) fail(Category::Semantic, s.loc, "cannot assign to constant " + quoted(s.name));
          fail(Category::Semantic, s.loc, "undefined variable " + quoted(s.name));
        }
        const ast::Local& target = fn_->locals[static_cast<size_t>(*local)];
        if (!target.isMutable) fail(Category::Semantic, s.loc, "cannot assign to immutable variable " + quoted(s.name));
        expectType(checkValue(*s.expr, target.type), target.type, s.expr->loc);
        s.local = *local;
        break;
      }
      case StmtKind::If:
        expectType(checkValue(*s.expr, Type::Bool), Type::Bool, s.expr->loc);
        checkBlock(s.body);
        if (s.hasElse) checkBlock(s.elseBody);
        break;
      case StmtKind::While:
        expectType(checkValue(*s.expr, Type::Bool), Type::Bool, s.expr->loc);
        ++loopDepth_;
        checkBlock(s.body);
        --loopDepth_;
        break;
      case StmtKind::Return:
        checkReturn(s);
        break;
      case StmtKind::Break:
      case StmtKind::Continue:
        if (loopDepth_ == 0) fail(Category::Semantic, s.loc, std::string(s.kind == StmtKind::Break ? "'break'" : "'continue'") + " outside of a loop");
        break;
      case StmtKind::Call:
        checkExpr(*s.expr, std::nullopt);
        break;
      case StmtKind::Block:
        checkBlock(s.body);
        break;
    }
  }

  void checkReturn(Stmt& s) {
    if (fn_->ret == Type::Void) {
      if (s.expr) fail(Category::Type, s.expr->loc, "a void function cannot return a value");
      return;
    }
    if (!s.expr) fail(Category::Type, s.loc, std::string("expected a return value of type ") + quoted(typeName(fn_->ret)));
    expectType(checkValue(*s.expr, fn_->ret), fn_->ret, s.expr->loc);
  }

  Type checkValue(Expr& e, std::optional<Type> expected) {
    Type t = checkExpr(e, expected);
    if (t == Type::Void) fail(Category::Type, e.loc, "expression has type 'void' and produces no value");
    return t;
  }

  Type checkExpr(Expr& e, std::optional<Type> expected) {
    switch (e.kind) {
      case ExprKind::IntLit: return e.type = checkIntLiteral(e, expected);
      case ExprKind::BoolLit:
        e.constBits = e.boolValue ? 1 : 0;
        return e.type = Type::Bool;
      case ExprKind::Ident: return e.type = checkIdent(e);
      case ExprKind::Unary: return e.type = checkUnary(e, expected);
      case ExprKind::Binary: return e.type = checkBinary(e, expected);
      case ExprKind::Call: return e.type = checkCall(e);
      case ExprKind::Cast: return e.type = checkCast(e);
    }
    return Type::Void;
  }

  Type checkIntLiteral(Expr& e, std::optional<Type> expected) {
    Type t;
    if (e.suffix) {
      t = *e.suffix;
    } else if (expected && isInteger(*expected)) {
      t = *expected;
    } else {
      fail(Category::Type, e.loc, "cannot infer the type of integer literal " + quoted(e.name) + "; add a suffix such as 'i32' or use it where a type is expected");
    }
    if (!literalFits(t, e.negative, e.magnitude)) {
      fail(Category::Type, e.loc, std::string("integer literal ") + (e.negative ? "-" : "") + e.name + " does not fit in type " + quoted(typeName(t)));
    }
    e.constBits = literalBits(t, e.negative, e.magnitude);
    return t;
  }

  Type checkIdent(Expr& e) {
    if (auto local = findLocal(e.name)) {
      e.local = *local;
      return fn_->locals[static_cast<size_t>(*local)].type;
    }
    auto it = consts_.find(e.name);
    if (it == consts_.end()) fail(Category::Semantic, e.loc, "undefined variable " + quoted(e.name));
    e.constBits = it->second.bits;
    return it->second.type;
  }

  Type checkUnary(Expr& e, std::optional<Type> expected) {
    Expr& operand = *e.operands[0];
    switch (e.unary) {
      case UnaryKind::Neg: {
        Type t = checkValue(operand, expected);
        if (!isInteger(t) || !isSigned(t)) fail(Category::Type, e.loc, "unary '-' requires a signed integer but found " + quoted(typeName(t)));
        return t;
      }
      case UnaryKind::Not: {
        Type t = checkValue(operand, Type::Bool);
        if (t != Type::Bool) fail(Category::Type, e.loc, "'!' requires 'bool' but found " + quoted(typeName(t)));
        return t;
      }
      case UnaryKind::BitNot: {
        Type t = checkValue(operand, expected);
        if (!isInteger(t)) fail(Category::Type, e.loc, "'~' requires an integer but found " + quoted(typeName(t)));
        return t;
      }
    }
    return Type::Void;
  }

  Type checkBinary(Expr& e, std::optional<Type> expected) {
    BinaryKind kind = e.binary;
    Expr& l = *e.operands[0];
    Expr& r = *e.operands[1];
    if (ast::isLogical(kind)) {
      Type lt = checkValue(l, Type::Bool);
      Type rt = checkValue(r, Type::Bool);
      if (lt != Type::Bool || rt != Type::Bool) {
        fail(Category::Type, e.loc, std::string("'") + ast::binaryText(kind) + "' requires 'bool' operands");
      }
      return Type::Bool;
    }
    std::optional<Type> hint = ast::isComparison(kind) ? std::nullopt : expected;
    Type lt;
    Type rt;
    if (isUntypedLiteral(l) && !isUntypedLiteral(r)) {
      rt = checkValue(r, hint);
      lt = checkValue(l, rt);
    } else {
      lt = checkValue(l, hint);
      rt = checkValue(r, lt);
    }
    if (lt != rt) {
      fail(Category::Type, e.loc, std::string("operands of '") + ast::binaryText(kind) + "' have different types " + quoted(typeName(lt)) + " and " + quoted(typeName(rt)) + "; use 'as' to convert explicitly");
    }
    bool equality = kind == BinaryKind::Eq || kind == BinaryKind::Ne;
    bool bitwise = kind == BinaryKind::BitAnd || kind == BinaryKind::BitOr || kind == BinaryKind::BitXor;
    bool allowed = isInteger(lt) || (lt == Type::Bool && (equality || bitwise));
    if (!allowed) {
      fail(Category::Type, e.loc, std::string("operator '") + ast::binaryText(kind) + "' is not supported for type " + quoted(typeName(lt)));
    }
    return ast::isComparison(kind) ? Type::Bool : lt;
  }

  Type checkCall(Expr& e) {
    auto it = functions_.find(e.name);
    if (it == functions_.end()) fail(Category::Semantic, e.loc, "undefined function " + quoted(e.name));
    const Signature& sig = it->second;
    if (e.operands.size() != sig.params.size()) {
      fail(Category::Semantic, e.loc, "function " + quoted(e.name) + " expects " + std::to_string(sig.params.size()) + " argument(s) but " + std::to_string(e.operands.size()) + " were given");
    }
    for (size_t i = 0; i < sig.params.size(); ++i) {
      Expr& arg = *e.operands[i];
      expectType(checkValue(arg, sig.params[i]), sig.params[i], arg.loc);
    }
    return sig.ret;
  }

  Type checkCast(Expr& e) {
    Type from = checkValue(*e.operands[0], std::nullopt);
    if (e.castTo == Type::Bool && from != Type::Bool) {
      fail(Category::Type, e.loc, "cannot convert " + quoted(typeName(from)) + " to 'bool'; compare with zero instead");
    }
    return e.castTo;
  }
};

}

void analyze(ast::Module& module) { Analyzer(module).run(); }

}
