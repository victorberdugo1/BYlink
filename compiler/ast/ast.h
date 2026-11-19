#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "common/diagnostic.h"
#include "types/types.h"

namespace bylink::ast {

enum class ExprKind { IntLit, BoolLit, Ident, Unary, Binary, Call, Cast };
enum class UnaryKind { Neg, Not, BitNot };
enum class BinaryKind {
  Add, Sub, Mul, Div, Rem, BitAnd, BitOr, BitXor,
  Eq, Ne, Lt, Le, Gt, Ge,
  LogAnd, LogOr
};

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct Expr {
  ExprKind kind = ExprKind::IntLit;
  Loc loc;
  Type type = Type::Void;
  bool negative = false;
  uint64_t magnitude = 0;
  std::optional<Type> suffix;
  bool boolValue = false;
  std::string name;
  UnaryKind unary = UnaryKind::Neg;
  BinaryKind binary = BinaryKind::Add;
  Type castTo = Type::Void;
  std::vector<ExprPtr> operands;
  int local = -1;
  std::optional<uint64_t> constBits;
};

enum class StmtKind { Let, Assign, If, While, Return, Break, Continue, Call, Block };

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;
using StmtList = std::vector<StmtPtr>;

struct Stmt {
  StmtKind kind = StmtKind::Block;
  Loc loc;
  std::string name;
  bool isMutable = false;
  Type declType = Type::Void;
  int local = -1;
  ExprPtr expr;
  StmtList body;
  StmtList elseBody;
  bool hasElse = false;
};

struct Param {
  std::string name;
  Type type = Type::Void;
  Loc loc;
};

struct Local {
  std::string name;
  Type type = Type::Void;
  bool isMutable = false;
};

struct Function {
  std::string name;
  Loc loc;
  std::vector<Param> params;
  Type ret = Type::Void;
  StmtList body;
  std::vector<Local> locals;
};

struct Const {
  std::string name;
  Loc loc;
  Type type = Type::Void;
  ExprPtr init;
  uint64_t bits = 0;
};

struct Module {
  std::string name;
  Loc loc;
  std::vector<Const> consts;
  std::vector<Function> functions;
};

bool terminates(const Stmt& stmt);

inline bool terminates(const StmtList& list) { return !list.empty() && terminates(*list.back()); }

inline bool terminates(const Stmt& stmt) {
  switch (stmt.kind) {
    case StmtKind::Return:
    case StmtKind::Break:
    case StmtKind::Continue: return true;
    case StmtKind::If: return stmt.hasElse && terminates(stmt.body) && terminates(stmt.elseBody);
    case StmtKind::Block: return terminates(stmt.body);
    default: return false;
  }
}

inline bool isComparison(BinaryKind kind) {
  return kind >= BinaryKind::Eq && kind <= BinaryKind::Ge;
}

inline bool isLogical(BinaryKind kind) { return kind == BinaryKind::LogAnd || kind == BinaryKind::LogOr; }

inline BinOp toBinOp(BinaryKind kind) {
  switch (kind) {
    case BinaryKind::Add: return BinOp::Add;
    case BinaryKind::Sub: return BinOp::Sub;
    case BinaryKind::Mul: return BinOp::Mul;
    case BinaryKind::Div: return BinOp::Div;
    case BinaryKind::Rem: return BinOp::Rem;
    case BinaryKind::BitAnd: return BinOp::And;
    case BinaryKind::BitOr: return BinOp::Or;
    default: return BinOp::Xor;
  }
}

inline CmpOp toCmpOp(BinaryKind kind) {
  switch (kind) {
    case BinaryKind::Eq: return CmpOp::Eq;
    case BinaryKind::Ne: return CmpOp::Ne;
    case BinaryKind::Lt: return CmpOp::Lt;
    case BinaryKind::Le: return CmpOp::Le;
    case BinaryKind::Gt: return CmpOp::Gt;
    default: return CmpOp::Ge;
  }
}

inline const char* binaryText(BinaryKind kind) {
  switch (kind) {
    case BinaryKind::Add: return "+";
    case BinaryKind::Sub: return "-";
    case BinaryKind::Mul: return "*";
    case BinaryKind::Div: return "/";
    case BinaryKind::Rem: return "%";
    case BinaryKind::BitAnd: return "&";
    case BinaryKind::BitOr: return "|";
    case BinaryKind::BitXor: return "^";
    case BinaryKind::Eq: return "==";
    case BinaryKind::Ne: return "!=";
    case BinaryKind::Lt: return "<";
    case BinaryKind::Le: return "<=";
    case BinaryKind::Gt: return ">";
    case BinaryKind::Ge: return ">=";
    case BinaryKind::LogAnd: return "&&";
    case BinaryKind::LogOr: return "||";
  }
  return "?";
}

}
