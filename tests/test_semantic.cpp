#include "harness.h"
#include "parser/parser.h"
#include "semantic/sema.h"

using namespace bylink;

namespace {

void accept(const std::string& body) {
  ast::Module m = parse("module t;\n" + body);
  analyze(m);
}

void reject(const std::string& body, Category category, const std::string& fragment) {
  try {
    accept(body);
  } catch (const CompileError& e) {
    CHECK(e.category() == category);
    if (std::string(e.what()).find(fragment) == std::string::npos) {
      harness::reportFailure(__FILE__, __LINE__, "message '" + std::string(e.what()) + "' lacks '" + fragment + "'");
    }
    return;
  }
  harness::reportFailure(__FILE__, __LINE__, "program was accepted: " + body);
}

}

TEST(sema_accepts_valid_programs) {
  accept("fn main() -> i32 { let x: i64 = 5; let y: i64 = x * 2 + 1; return (y as i32) - 1; }");
  accept("const A: u8 = 200; const B: u8 = A + 55; fn main() -> i32 { return B as i32; }");
  accept("fn f(a: bool) -> bool { return !a || a && true; }");
  accept("fn main() -> i32 { let mut i: i32 = 0; while true { if i > 3 { break; } i = i + 1; } return i; }");
  accept("fn a() -> i32 { return b(); } fn b() -> i32 { return 1; }");
  accept("fn f(x: i8) -> i8 { return -x; } fn g() -> i8 { return -128; }");
}

TEST(sema_type_errors) {
  reject("fn f() -> i32 { let x: i32 = 1; let y: i64 = x; return 0; }", Category::Type, "expected 'i64' but found 'i32'");
  reject("fn f(a: i32, b: i64) -> i32 { return a + b; }", Category::Type, "different types");
  reject("fn f() -> i32 { return true; }", Category::Type, "expected 'i32' but found 'bool'");
  reject("fn f() -> i32 { return 300u8; }", Category::Type, "does not fit");
  reject("fn f() -> i8 { return 128; }", Category::Type, "does not fit");
  reject("fn f() -> u8 { return -1; }", Category::Type, "does not fit");
  reject("fn f(a: u32) -> u32 { return -a; }", Category::Type, "signed integer");
  reject("fn f(a: i32) -> bool { return a; }", Category::Type, "expected 'bool'");
  reject("fn f(a: i32) -> i32 { if a { return 1; } return 0; }", Category::Type, "expected 'bool'");
  reject("fn f() -> bool { return 1 < 2; }", Category::Type, "cannot infer");
  reject("fn f(a: i32) -> bool { return a as bool; }", Category::Type, "compare with zero");
  reject("fn f(a: bool, b: bool) -> bool { return a < b; }", Category::Type, "not supported for type 'bool'");
  reject("fn f() { return 1; }", Category::Type, "void function");
  reject("fn f() -> i32 { return; }", Category::Type, "expected a return value");
  reject("fn g() { } fn f() -> i32 { return g(); }", Category::Type, "'void'");
  reject("fn f(a: i32) -> i32 { return f(true); }", Category::Type, "expected 'i32' but found 'bool'");
}

TEST(sema_semantic_errors) {
  reject("fn f() -> i32 { return x; }", Category::Semantic, "undefined variable 'x'");
  reject("fn f() -> i32 { return g(); }", Category::Semantic, "undefined function 'g'");
  reject("fn f() -> i32 { let x: i32 = 1; x = 2; return x; }", Category::Semantic, "immutable");
  reject("const K: i32 = 1; fn f() -> i32 { K = 2; return K; }", Category::Semantic, "constant");
  reject("fn f() -> i32 { let a: i32 = 1; let a: i32 = 2; return a; }", Category::Semantic, "already declared");
  reject("fn f(a: i32, a: i32) -> i32 { return a; }", Category::Semantic, "already declared");
  reject("fn f() { } fn f() { }", Category::Semantic, "already defined");
  reject("fn f(a: i32) -> i32 { return f(1, 2); }", Category::Semantic, "expects 1 argument");
  reject("fn f() { break; }", Category::Semantic, "outside of a loop");
  reject("fn f() -> i32 { return 1; return 2; }", Category::Semantic, "unreachable");
  reject("fn f(a: bool) -> i32 { if a { return 1; } }", Category::Semantic, "every path");
  reject("fn f() -> i32 { while true { return 1; } }", Category::Semantic, "every path");
  reject("fn main(a: i32) -> i32 { return a; }", Category::Semantic, "'main' must have");
  reject("fn main() { }", Category::Semantic, "'main' must have");
  reject("const K: i32 = 1 / 0; fn f() { }", Category::Semantic, "division by zero");
  reject("fn g() -> i32 { return 1; } const K: i32 = g(); fn f() { }", Category::Semantic, "not a compile-time constant");
}

TEST(sema_scoping) {
  accept("fn f() -> i32 { let a: i32 = 1; { let a: i32 = 2; } return a; }");
  reject("fn f() -> i32 { { let b: i32 = 2; } return b; }", Category::Semantic, "undefined variable 'b'");
}
