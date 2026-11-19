#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "driver/pipeline.h"
#include "harness.h"

#ifndef _WIN32
#include <sys/wait.h>
#endif

using namespace bylink;
namespace fs = std::filesystem;

namespace {

struct RunResult {
  bool exited = false;
  int code = -1;
  int rawStatus = 0;
};

fs::path scratch() {
  fs::path dir = fs::temp_directory_path() / "bylink-tests";
  fs::create_directories(dir);
  return dir;
}

RunResult run(const std::string& command) {
  RunResult r;
  r.rawStatus = std::system(command.c_str());
#ifdef _WIN32
  r.exited = true;
  r.code = r.rawStatus;
#else
  r.exited = WIFEXITED(r.rawStatus);
  r.code = r.exited ? WEXITSTATUS(r.rawStatus) : -1;
#endif
  return r;
}

std::string quoted(const fs::path& p) { return "\"" + p.string() + "\""; }

std::string exeName(const std::string& name) {
#ifdef _WIN32
  return name + ".exe";
#else
  return name;
#endif
}

RunResult compileAndRun(const std::string& source, OptLevel level, const std::string& name) {
  fs::path exe = scratch() / exeName(name);
  fs::remove(exe);
  BuildOptions options;
  options.opt = level;
  buildExecutable(source, name + ".by", exe.string(), options);
  return run(quoted(exe));
}

std::string readFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void expectExit(const std::string& body, int expected, const std::string& name) {
  const OptLevel levels[] = {OptLevel::O0, OptLevel::O1, OptLevel::O2, OptLevel::O3, OptLevel::Os};
  for (OptLevel level : levels) {
    RunResult r = compileAndRun("module t;\n" + body, level, name);
    if (!r.exited || r.code != expected) {
      harness::reportFailure(name.c_str(), 0,
                             std::string("at -") + optFlag(level) + " exit code " + std::to_string(r.code) + " expected " + std::to_string(expected));
    }
  }
}

}

TEST(native_return_constant) { expectExit("fn main() -> i32 { return 42; }", 42, "n_const"); }

TEST(native_arithmetic) {
  expectExit("fn main() -> i32 { let a: i32 = 17; let b: i32 = 5; return a * b + a / b - a % b; }", 86, "n_arith");
}

TEST(native_wraparound_and_casts) {
  expectExit("fn main() -> i32 { let a: u8 = 250; let b: u8 = a + 10; return b as i32; }", 4, "n_wrap");
  expectExit("fn main() -> i32 { let a: i32 = 300; return (a as u8) as i32; }", 44, "n_trunc");
  expectExit("fn main() -> i32 { let a: i8 = -1; return (a as i32) + 101; }", 100, "n_sext");
  expectExit("fn main() -> i32 { let a: u8 = 255; return ((a as i32) == 255) as i32 + 20; }", 21, "n_zext");
  expectExit("fn main() -> i32 { let a: i32 = -1; let b: u32 = a as u32; return (b > 5u32) as i32 + 7; }", 8, "n_unsigned_cmp");
}

TEST(native_functions_and_recursion) {
  expectExit("fn fib(n: i32) -> i32 { if n < 2 { return n; } return fib(n - 1) + fib(n - 2); } fn main() -> i32 { return fib(10); }", 55, "n_fib");
  expectExit("fn add(a: i32, b: i32) -> i32 { return a + b; } fn main() -> i32 { return add(40, 2); }", 42, "n_add");
}

TEST(native_loops) {
  expectExit("fn main() -> i32 { let mut s: i32 = 0; let mut i: i32 = 1; while i <= 10 { s = s + i; i = i + 1; } return s; }", 55, "n_while");
  expectExit("fn main() -> i32 { let mut s: i32 = 0; let mut i: i32 = 0; while true { i = i + 1; if i > 10 { break; } if i % 2 == 0 { continue; } s = s + i; } return s; }", 25, "n_break");
}

TEST(native_conditionals) {
  expectExit("fn pick(a: i32) -> i32 { if a == 1 { return 10; } else if a == 2 { return 20; } else { return 30; } } fn main() -> i32 { return pick(1) + pick(2) + pick(3) - 50; }", 10, "n_elseif");
}

TEST(native_short_circuit_avoids_trap) {
  expectExit("fn main() -> i32 { let zero: i32 = 0; if false && (1 / zero == 0) { return 1; } if true || (1 / zero == 0) { return 7; } return 2; }", 7, "n_shortcircuit");
}

TEST(native_constants) {
  expectExit("const A: i32 = 6 * 7; const B: i32 = A - 2; fn main() -> i32 { return B; }", 40, "n_consts");
}

TEST(native_bitwise_and_unary) {
  expectExit("fn main() -> i32 { let a: i32 = 12; return (a & 10) + (a | 3) + (a ^ 5) + (~a) + 1 + (-a) + 12; }", 8 + 15 + 9 - 13 + 1 - 12 + 12, "n_bits");
}

TEST(native_division_by_zero_traps) {
  RunResult r = compileAndRun("module t;\nfn main() -> i32 { let zero: i32 = 0; return 1 / zero; }\n", OptLevel::O0, "n_trap");
  CHECK(r.rawStatus != 0);
#ifndef _WIN32
  CHECK(!r.exited || r.code > 128);
#endif
}

TEST(native_signed_overflow_division_traps) {
  RunResult r = compileAndRun("module t;\nfn main() -> i32 { let a: i32 = -2147483648; let b: i32 = -1; return a / b; }\n", OptLevel::O2, "n_trap_min");
  CHECK(r.rawStatus != 0);
}

TEST(native_examples) {
  struct Example {
    const char* file;
    int expected;
  };
  const Example examples[] = {{"hello.by", 42}, {"arithmetic.by", 54}, {"functions.by", 55}, {"control_flow.by", 56}};
  for (const Example& example : examples) {
    fs::path path = fs::path(BYLINK_EXAMPLES_DIR) / example.file;
    RunResult r = compileAndRun(readFile(path), OptLevel::O2, std::string("ex_") + example.file);
    CHECK(r.exited);
    CHECK_EQ(r.code, example.expected);
  }
}

TEST(cli_end_to_end) {
  fs::path exe = scratch() / exeName("cli_hello");
  fs::remove(exe);
  fs::path source = fs::path(BYLINK_EXAMPLES_DIR) / "hello.by";
  RunResult build = run(quoted(BYLC_PATH) + " " + quoted(source) + " -O3 -o " + quoted(exe));
  CHECK_EQ(build.code, 0);
  RunResult executed = run(quoted(exe));
  CHECK_EQ(executed.code, 42);
}

TEST(cli_diagnostics_and_exit_codes) {
  fs::path bad = scratch() / "bad.by";
  std::ofstream(bad) << "module bad;\nfn main() -> i32 {\n  return true;\n}\n";
  fs::path err = scratch() / "bad.err";
  RunResult r = run(quoted(BYLC_PATH) + " " + quoted(bad) + " -o " + quoted(scratch() / "bad_out") + " 2> " + quoted(err));
  CHECK_EQ(r.code, 1);
  std::string message = readFile(err);
  CHECK(message.find(":3:10: error[type]:") != std::string::npos);
  RunResult usage = run(quoted(BYLC_PATH) + " 2> " + quoted(err));
  CHECK_EQ(usage.code, 2);
  RunResult target = run(quoted(BYLC_PATH) + " " + quoted(fs::path(BYLINK_EXAMPLES_DIR) / "hello.by") + " --target riscv32-unknown-elf -o " + quoted(scratch() / "t_out") + " 2> " + quoted(err));
  CHECK_EQ(target.code, 2);
  CHECK(readFile(err).find("unsupported target") != std::string::npos);
  CHECK(!fs::exists(scratch() / "t_out"));
}

TEST(cli_emit_modes) {
  fs::path source = fs::path(BYLINK_EXAMPLES_DIR) / "functions.by";
  fs::path ll = scratch() / "functions.ll";
  fs::path ir = scratch() / "functions.byir";
  CHECK_EQ(run(quoted(BYLC_PATH) + " " + quoted(source) + " --emit-llvm -o " + quoted(ll)).code, 0);
  CHECK_EQ(run(quoted(BYLC_PATH) + " " + quoted(source) + " --emit-ir -o " + quoted(ir)).code, 0);
  CHECK(readFile(ll).find("define i32 @main()") != std::string::npos);
  CHECK(readFile(ir).find("func @fib(i32 %0) -> i32") != std::string::npos);
  CHECK_EQ(run(quoted(BYLC_PATH) + " --version").code, 0);
}
