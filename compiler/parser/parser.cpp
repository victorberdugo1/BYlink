#include "parser/parser.h"

#include <utility>

#include "lexer/lexer.h"

namespace bylink {

namespace {

using ast::BinaryKind;
using ast::Expr;
using ast::ExprKind;
using ast::ExprPtr;
using ast::Stmt;
using ast::StmtKind;
using ast::StmtList;
using ast::StmtPtr;
using ast::UnaryKind;

struct BinaryInfo {
  BinaryKind kind;
  int precedence;
};

std::optional<BinaryInfo> binaryInfo(Tok tok) {
  switch (tok) {
    case Tok::PipePipe: return BinaryInfo{BinaryKind::LogOr, 1};
    case Tok::AmpAmp: return BinaryInfo{BinaryKind::LogAnd, 2};
    case Tok::EqEq: return BinaryInfo{BinaryKind::Eq, 3};
    case Tok::NotEq: return BinaryInfo{BinaryKind::Ne, 3};
    case Tok::Lt: return BinaryInfo{BinaryKind::Lt, 3};
    case Tok::Le: return BinaryInfo{BinaryKind::Le, 3};
    case Tok::Gt: return BinaryInfo{BinaryKind::Gt, 3};
    case Tok::Ge: return BinaryInfo{BinaryKind::Ge, 3};
    case Tok::Pipe: return BinaryInfo{BinaryKind::BitOr, 4};
    case Tok::Caret: return BinaryInfo{BinaryKind::BitXor, 5};
    case Tok::Amp: return BinaryInfo{BinaryKind::BitAnd, 6};
    case Tok::Plus: return BinaryInfo{BinaryKind::Add, 7};
    case Tok::Minus: return BinaryInfo{BinaryKind::Sub, 7};
    case Tok::Star: return BinaryInfo{BinaryKind::Mul, 8};
    case Tok::Slash: return BinaryInfo{BinaryKind::Div, 8};
    case Tok::Percent: return BinaryInfo{BinaryKind::Rem, 8};
    default: return std::nullopt;
  }
}

constexpr size_t kMaxNesting = 256;

class Parser {
 public:
  explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

  ast::Module parseModule() {
    ast::Module module;
    const Token& header = expect(Tok::KwModule, "'module'");
    module.loc = header.loc;
    module.name = expect(Tok::Ident, "a module name").text;
    expect(Tok::Semi, "';'");
    while (peek().kind != Tok::End) {
      if (peek().kind == Tok::KwFn) {
        module.functions.push_back(parseFunction());
      } else if (peek().kind == Tok::KwConst) {
        module.consts.push_back(parseConst());
      } else {
        fail(peek(), "expected 'fn' or 'const' but found '" + peek().text + "'");
      }
    }
    return module;
  }

 private:
  std::vector<Token> tokens_;
  size_t pos_ = 0;
  size_t depth_ = 0;

  class DepthGuard {
   public:
    DepthGuard(Parser& parser, const Token& at) : parser_(parser) {
      if (++parser_.depth_ > kMaxNesting) parser_.fail(at, "nesting is too deep (limit " + std::to_string(kMaxNesting) + ")");
    }
    DepthGuard(const DepthGuard&) = delete;
    DepthGuard& operator=(const DepthGuard&) = delete;
    ~DepthGuard() { --parser_.depth_; }

   private:
    Parser& parser_;
  };

  const Token& peek(size_t ahead = 0) const {
    size_t index = pos_ + ahead;
    return tokens_[index < tokens_.size() ? index : tokens_.size() - 1];
  }

  const Token& advance() {
    const Token& token = tokens_[pos_];
    if (pos_ + 1 < tokens_.size()) ++pos_;
    return token;
  }

  bool match(Tok kind) {
    if (peek().kind != kind) return false;
    advance();
    return true;
  }

  [[noreturn]] void fail(const Token& at, const std::string& message) const {
    throw CompileError(Category::Syntax, at.loc, message);
  }

  const Token& expect(Tok kind, const std::string& what) {
    if (peek().kind != kind) fail(peek(), "expected " + what + " but found '" + peek().text + "'");
    return advance();
  }

  Type parseType(bool allowVoid) {
    const Token& token = expect(Tok::TypeName, "a type");
    if (!allowVoid && *token.type == Type::Void) fail(token, "'void' is only allowed as a return type");
    return *token.type;
  }

  ast::Const parseConst() {
    ast::Const c;
    c.loc = advance().loc;
    c.name = expect(Tok::Ident, "a constant name").text;
    expect(Tok::Colon, "':'");
    c.type = parseType(false);
    expect(Tok::Assign, "'='");
    c.init = parseExpr();
    expect(Tok::Semi, "';'");
    return c;
  }

  ast::Function parseFunction() {
    ast::Function f;
    f.loc = advance().loc;
    f.name = expect(Tok::Ident, "a function name").text;
    expect(Tok::LParen, "'('");
    if (peek().kind != Tok::RParen) {
      do {
        ast::Param p;
        p.loc = peek().loc;
        p.name = expect(Tok::Ident, "a parameter name").text;
        expect(Tok::Colon, "':'");
        p.type = parseType(false);
        f.params.push_back(std::move(p));
      } while (match(Tok::Comma));
    }
    expect(Tok::RParen, "')'");
    f.ret = match(Tok::Arrow) ? parseType(true) : Type::Void;
    f.body = parseBlock();
    return f;
  }

  StmtList parseBlock() {
    DepthGuard guard(*this, peek());
    expect(Tok::LBrace, "'{'");
    StmtList list;
    while (peek().kind != Tok::RBrace) {
      if (peek().kind == Tok::End) fail(peek(), "expected '}' but found end of file");
      list.push_back(parseStmt());
    }
    advance();
    return list;
  }

  StmtPtr makeStmt(StmtKind kind, Loc loc) {
    auto s = std::make_unique<Stmt>();
    s->kind = kind;
    s->loc = loc;
    return s;
  }

  StmtPtr parseStmt() {
    const Token& first = peek();
    switch (first.kind) {
      case Tok::KwLet: return parseLet();
      case Tok::KwIf: return parseIf();
      case Tok::KwWhile: return parseWhile();
      case Tok::KwReturn: return parseReturn();
      case Tok::KwBreak:
      case Tok::KwContinue: {
        StmtPtr s = makeStmt(first.kind == Tok::KwBreak ? StmtKind::Break : StmtKind::Continue, first.loc);
        advance();
        expect(Tok::Semi, "';'");
        return s;
      }
      case Tok::LBrace: {
        StmtPtr s = makeStmt(StmtKind::Block, first.loc);
        s->body = parseBlock();
        return s;
      }
      case Tok::Ident:
        if (peek(1).kind == Tok::Assign) return parseAssign();
        return parseCallStmt();
      default: fail(first, "expected a statement but found '" + first.text + "'");
    }
  }

  StmtPtr parseLet() {
    StmtPtr s = makeStmt(StmtKind::Let, advance().loc);
    s->isMutable = match(Tok::KwMut);
    s->name = expect(Tok::Ident, "a variable name").text;
    expect(Tok::Colon, "':' (variables need an explicit type)");
    s->declType = parseType(false);
    expect(Tok::Assign, "'='");
    s->expr = parseExpr();
    expect(Tok::Semi, "';'");
    return s;
  }

  StmtPtr parseAssign() {
    const Token& name = advance();
    StmtPtr s = makeStmt(StmtKind::Assign, name.loc);
    s->name = name.text;
    advance();
    s->expr = parseExpr();
    expect(Tok::Semi, "';'");
    return s;
  }

  StmtPtr parseCallStmt() {
    StmtPtr s = makeStmt(StmtKind::Call, peek().loc);
    s->expr = parseExpr();
    if (s->expr->kind != ExprKind::Call) throw CompileError(Category::Syntax, s->loc, "expression statements must be function calls");
    expect(Tok::Semi, "';'");
    return s;
  }

  StmtPtr parseIf() {
    DepthGuard guard(*this, peek());
    StmtPtr s = makeStmt(StmtKind::If, advance().loc);
    s->expr = parseExpr();
    s->body = parseBlock();
    if (match(Tok::KwElse)) {
      s->hasElse = true;
      if (peek().kind == Tok::KwIf) {
        s->elseBody.push_back(parseIf());
      } else {
        s->elseBody = parseBlock();
      }
    }
    return s;
  }

  StmtPtr parseWhile() {
    StmtPtr s = makeStmt(StmtKind::While, advance().loc);
    s->expr = parseExpr();
    s->body = parseBlock();
    return s;
  }

  StmtPtr parseReturn() {
    StmtPtr s = makeStmt(StmtKind::Return, advance().loc);
    if (peek().kind != Tok::Semi) s->expr = parseExpr();
    expect(Tok::Semi, "';'");
    return s;
  }

  ExprPtr makeExpr(ExprKind kind, Loc loc) {
    auto e = std::make_unique<Expr>();
    e->kind = kind;
    e->loc = loc;
    return e;
  }

  ExprPtr parseExpr() { return parseBinary(1); }

  ExprPtr parseBinary(int minPrecedence) {
    DepthGuard guard(*this, peek());
    ExprPtr lhs = parseCast();
    for (size_t chain = 0;; ++chain) {
      auto info = binaryInfo(peek().kind);
      if (!info || info->precedence < minPrecedence) return lhs;
      if (depth_ + chain >= kMaxNesting) fail(peek(), "nesting is too deep (limit " + std::to_string(kMaxNesting) + ")");
      Loc loc = advance().loc;
      ExprPtr rhs = parseBinary(info->precedence + 1);
      ExprPtr e = makeExpr(ExprKind::Binary, loc);
      e->binary = info->kind;
      e->operands.push_back(std::move(lhs));
      e->operands.push_back(std::move(rhs));
      lhs = std::move(e);
    }
  }

  ExprPtr parseCast() {
    ExprPtr e = parseUnary();
    for (size_t chain = 0; peek().kind == Tok::KwAs; ++chain) {
      if (depth_ + chain >= kMaxNesting) fail(peek(), "nesting is too deep (limit " + std::to_string(kMaxNesting) + ")");
      Loc loc = advance().loc;
      ExprPtr cast = makeExpr(ExprKind::Cast, loc);
      cast->castTo = parseType(false);
      cast->operands.push_back(std::move(e));
      e = std::move(cast);
    }
    return e;
  }

  ExprPtr parseUnary() {
    const Token& token = peek();
    if (token.kind == Tok::Minus && peek(1).kind == Tok::Int) {
      advance();
      ExprPtr literal = parseIntLiteral();
      literal->negative = true;
      literal->loc = token.loc;
      return literal;
    }
    std::optional<UnaryKind> kind;
    if (token.kind == Tok::Minus) kind = UnaryKind::Neg;
    if (token.kind == Tok::Bang) kind = UnaryKind::Not;
    if (token.kind == Tok::Tilde) kind = UnaryKind::BitNot;
    if (!kind) return parsePrimary();
    DepthGuard guard(*this, token);
    advance();
    ExprPtr e = makeExpr(ExprKind::Unary, token.loc);
    e->unary = *kind;
    e->operands.push_back(parseUnary());
    return e;
  }

  ExprPtr parseIntLiteral() {
    const Token& token = advance();
    ExprPtr e = makeExpr(ExprKind::IntLit, token.loc);
    e->magnitude = token.magnitude;
    e->suffix = token.type;
    e->name = token.text;
    return e;
  }

  ExprPtr parsePrimary() {
    const Token& token = peek();
    switch (token.kind) {
      case Tok::Int: return parseIntLiteral();
      case Tok::KwTrue:
      case Tok::KwFalse: {
        ExprPtr e = makeExpr(ExprKind::BoolLit, token.loc);
        e->boolValue = token.kind == Tok::KwTrue;
        advance();
        return e;
      }
      case Tok::Ident: {
        advance();
        if (peek().kind != Tok::LParen) {
          ExprPtr e = makeExpr(ExprKind::Ident, token.loc);
          e->name = token.text;
          return e;
        }
        advance();
        ExprPtr call = makeExpr(ExprKind::Call, token.loc);
        call->name = token.text;
        if (peek().kind != Tok::RParen) {
          do {
            call->operands.push_back(parseExpr());
          } while (match(Tok::Comma));
        }
        expect(Tok::RParen, "')'");
        return call;
      }
      case Tok::LParen: {
        advance();
        ExprPtr e = parseExpr();
        expect(Tok::RParen, "')'");
        return e;
      }
      default: fail(token, "expected an expression but found '" + token.text + "'");
    }
  }
};

}

ast::Module parse(const std::string& source) { return Parser(tokenize(source)).parseModule(); }

}
