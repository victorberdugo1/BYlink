#include "harness.h"
#include "parser/parser.h"

using namespace bylink;

namespace {

CompileError parseError(const std::string& source) {
  try {
    parse(source);
  } catch (const CompileError& e) {
    return e;
  }
  throw std::runtime_error("expected a parse error");
}

}

TEST(parser_module_structure) {
  ast::Module m = parse(
      "module m;\nconst K: i32 = 1;\nfn f(a: i32, b: bool) -> i32 { return a; }\nfn g() { }\n");
  CHECK_EQ(m.name, "m");
  CHECK_EQ(m.consts.size(), 1u);
  CHECK_EQ(m.functions.size(), 2u);
  CHECK_EQ(m.functions[0].params.size(), 2u);
  CHECK(m.functions[0].ret == Type::I32);
  CHECK(m.functions[1].ret == Type::Void);
}

TEST(parser_precedence) {
  ast::Module m = parse("module m;\nfn f() -> bool { return 1 + 2 * 3 == 7 && true; }\n");
  const ast::Expr& e = *m.functions[0].body[0]->expr;
  CHECK(e.binary == ast::BinaryKind::LogAnd);
  const ast::Expr& eq = *e.operands[0];
  CHECK(eq.binary == ast::BinaryKind::Eq);
  const ast::Expr& sum = *eq.operands[0];
  CHECK(sum.binary == ast::BinaryKind::Add);
  CHECK(sum.operands[1]->binary == ast::BinaryKind::Mul);
}

TEST(parser_negative_literal_folding) {
  ast::Module m = parse("module m;\nfn f() -> i32 { return -5; }\n");
  const ast::Expr& e = *m.functions[0].body[0]->expr;
  CHECK(e.kind == ast::ExprKind::IntLit);
  CHECK(e.negative);
}

TEST(parser_else_if_chain) {
  ast::Module m = parse("module m;\nfn f(a: i32) -> i32 { if a == 1 { return 1; } else if a == 2 { return 2; } else { return 3; } }\n");
  const ast::Stmt& s = *m.functions[0].body[0];
  CHECK(s.hasElse);
  CHECK_EQ(s.elseBody.size(), 1u);
  CHECK(s.elseBody[0]->kind == ast::StmtKind::If);
}

TEST(parser_syntax_errors) {
  CompileError missingSemi = parseError("module m;\nfn f() -> i32 {\n  return 1\n}\n");
  CHECK(missingSemi.category() == Category::Syntax);
  CHECK_EQ(missingSemi.loc().line, 4u);
  CHECK_EQ(missingSemi.loc().column, 1u);
  CHECK(parseError("fn f() {}").category() == Category::Syntax);
  CHECK(parseError("module m;\nfn f() { let x = 1; }").category() == Category::Syntax);
  CHECK(parseError("module m;\nfn f() { 1 + 2; }").category() == Category::Syntax);
  CHECK(parseError("module m;\nfn f(a: void) { }").category() == Category::Syntax);
  CHECK(parseError("module m;\nfn f() {").category() == Category::Syntax);
}
