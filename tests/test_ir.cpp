#include "common/diagnostic.h"
#include "driver/pipeline.h"
#include "harness.h"

using namespace bylink;

namespace {

bool contains(const std::string& text, const std::string& fragment) { return text.find(fragment) != std::string::npos; }

std::string irText(const std::string& body, OptLevel level) { return ir::print(compileToIR("module t;\n" + body, level)); }

}

TEST(ir_unoptimized_shape) {
  std::string text = irText("fn main() -> i32 { return 40 + 2; }", OptLevel::O0);
  CHECK(contains(text, "byir 0"));
  CHECK(contains(text, "module t"));
  CHECK(contains(text, "func @main() -> i32 {"));
  CHECK(contains(text, "add i32 40, 2"));
}

TEST(ir_constant_folding) {
  std::string text = irText("fn main() -> i32 { return 40 + 2 * 1; }", OptLevel::O2);
  CHECK(contains(text, "ret i32 42"));
  CHECK(!contains(text, "add"));
  CHECK(!contains(text, "mul"));
}

TEST(ir_wrapping_folds) {
  std::string text = irText("fn main() -> i32 { return (250u8 + 10u8) as i32; }", OptLevel::O2);
  CHECK(contains(text, "ret i32 4"));
}

TEST(ir_division_by_zero_not_folded) {
  std::string text = irText("fn main() -> i32 { return 1 / (1 - 1); }", OptLevel::O2);
  CHECK(contains(text, "div i32 1, 0"));
}

TEST(ir_branch_simplification) {
  std::string text = irText("const N: i32 = 3; fn main() -> i32 { if N > 5 { return 1; } return 2; }", OptLevel::O2);
  CHECK(!contains(text, "condbr"));
  CHECK(!contains(text, "ret i32 1"));
  CHECK(contains(text, "ret i32 2"));
}

TEST(ir_calls_preserved) {
  std::string text = irText("fn g() { } fn main() -> i32 { g(); return 0; }", OptLevel::O2);
  CHECK(contains(text, "call void @g()"));
}

TEST(llvm_emission) {
  BuildOptions options;
  options.opt = OptLevel::O0;
  std::string ll = compileToLLVM("module t;\nfn main() -> i32 { let a: i32 = 7; return a / 2; }\n", "t.by", options);
  CHECK(contains(ll, "define i32 @main()"));
  CHECK(contains(ll, "sdiv i32"));
  CHECK(contains(ll, "@llvm.trap()"));
  CHECK(contains(ll, "target triple"));
}

TEST(llvm_internal_linkage_and_mangling) {
  BuildOptions options;
  std::string ll = compileToLLVM("module t;\nfn helper() -> i32 { return 1; }\nfn main() -> i32 { return helper(); }\n", "t.by", options);
  CHECK(contains(ll, "define internal i32 @\"t.helper\"()"));
}

TEST(llvm_rejects_foreign_target) {
  BuildOptions options;
  options.target = "riscv32-unknown-elf";
  try {
    compileToLLVM("module t;\nfn main() -> i32 { return 0; }\n", "t.by", options);
    CHECK(false);
  } catch (const CompileError& e) {
    CHECK(e.category() == Category::Toolchain);
    CHECK(contains(e.what(), "unsupported target"));
  }
}
