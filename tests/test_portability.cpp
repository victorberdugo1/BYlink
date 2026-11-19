#include <filesystem>
#include <functional>
#include <fstream>
#include <sstream>

#include "ai/byb.h"
#include "codegen/llvm_emitter.h"
#include "common/diagnostic.h"
#include "common/process.h"
#include "driver/pipeline.h"
#include "harness.h"
#include "ir/verify.h"
#include "target/target.h"

using namespace bylink;
namespace fs = std::filesystem;

namespace {

std::string bytes(std::initializer_list<int> values) {
  std::string out;
  for (int v : values) out.push_back(static_cast<char>(v));
  return out;
}

const std::string kHeader = bytes({'B', 'Y', 'I', 'R', 0});

bool decodeFails(const std::string& data) {
  try {
    ai::decodeBinary(data);
  } catch (const CompileError&) {
    return true;
  }
  return false;
}

std::string errorOf(const std::function<void()>& body) {
  try {
    body();
  } catch (const CompileError& e) {
    return e.what();
  }
  return "";
}

fs::path scratch() {
  fs::path dir = fs::temp_directory_path() / "bylink-portability";
  fs::create_directories(dir);
  return dir;
}

std::string readBinary(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string example(const char* name) {
  std::ifstream in(fs::path(BYLINK_EXAMPLES_DIR) / name, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

bool toolAvailable(const std::string& tool) { return runProcess({tool, "--version"}).started; }

bool skipUnlessClang() {
  if (toolAvailable("clang")) return false;
  harness::skip("clang is not installed");
  return true;
}

uint32_t le(const std::string& data, size_t offset, size_t width) {
  uint32_t v = 0;
  for (size_t i = 0; i < width; ++i) v |= static_cast<uint32_t>(static_cast<uint8_t>(data.at(offset + i))) << (8 * i);
  return v;
}

void expectObjectFormat(const Target& target, const std::string& object) {
  switch (target.objectFormat) {
    case ObjectFormat::Elf: {
      CHECK_EQ(object.substr(0, 4), bytes({0x7F, 'E', 'L', 'F'}));
      CHECK_EQ(static_cast<int>(object[4]), target.pointerBits == 64 ? 2 : 1);
      uint32_t machine = le(object, 18, 2);
      switch (target.arch) {
        case Arch::X86_64: CHECK_EQ(machine, 62u); break;
        case Arch::AArch64: CHECK_EQ(machine, 183u); break;
        case Arch::Arm32: CHECK_EQ(machine, 40u); break;
        case Arch::RiscV64:
        case Arch::RiscV32: CHECK_EQ(machine, 243u); break;
      }
      break;
    }
    case ObjectFormat::Coff:
      CHECK_EQ(le(object, 0, 2), target.arch == Arch::X86_64 ? 0x8664u : 0xAA64u);
      break;
    case ObjectFormat::MachO:
      CHECK_EQ(le(object, 0, 4), 0xFEEDFACFu);
      CHECK_EQ(le(object, 4, 4), target.arch == Arch::X86_64 ? 0x01000007u : 0x0100000Cu);
      break;
  }
}

}

TEST(byb_golden_bytes_are_pinned_by_the_specification) {
  std::string hello = compileToByb("module hello;\nfn main() -> i32 { return 42; }\n", OptLevel::O0);
  CHECK_EQ(hello, kHeader + bytes({5, 'h', 'e', 'l', 'l', 'o', 1, 4, 'm', 'a', 'i', 'n', 0, 2, 0, 0, 1, 1, 8, 2, 5, 0x54}));

  std::string sub = kHeader + bytes({1, 'm', 1, 4, 'm', 'a', 'i', 'n', 0, 2, 0, 1, 1, 2, 0, 2, 1, 0, 5, 0x0E, 5, 0x06, 8, 2, 4, 0});
  ir::Module module = ai::decodeBinary(sub);
  CHECK_EQ(module.functions[0].blocks[0].instrs[0].bin == BinOp::Sub, true);
  CHECK_EQ(ai::encodeBinary(module), sub);

  std::string wide = kHeader + bytes({1, 'm', 1, 1, 'f', 0, 7, 0, 0, 1, 1, 8, 7, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 1});
  CHECK_EQ(ai::encodeBinary(ai::decodeBinary(wide)), wide);
}

TEST(byb_decoder_rejects_non_canonical_and_hostile_encodings) {
  std::string good = kHeader + bytes({1, 'm', 1, 1, 'f', 0, 7, 0, 0, 1, 1, 8, 7, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 1});
  CHECK(!decodeFails(good));
  CHECK(decodeFails(kHeader + bytes({0x81, 0x00, 'm'})));
  CHECK(decodeFails(bytes({'B', 'Y', 'I', 'R', 1}) + good.substr(5)));
  CHECK(decodeFails(kHeader + bytes({1, 'm', 0xFF, 0xFF, 0xFF, 0xFF, 0x0F})));
  CHECK(decodeFails(kHeader + bytes({1, 'm', 1, 3, 'a', '"', 'b', 0, 7, 0, 0, 1, 1, 8, 7, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 1})));
  CHECK(decodeFails(kHeader + bytes({1, '1', 1, 1, 'f', 0, 7, 0, 0, 1, 1, 8, 7, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 1})));
  CHECK(decodeFails(kHeader + bytes({1, 'm', 1, 1, 'f', 0, 7, 0, 0, 1, 1, 9, 7, 0x0F})));
  CHECK(decodeFails(kHeader + bytes({1, 'm', 1, 1, 'f', 0, 7, 0, 0, 1, 1, 6, 5})));
  CHECK(decodeFails(kHeader + bytes({1, 'm', 1, 1, 'f', 0, 10, 0, 0, 1, 1, 8, 10})));
}

TEST(byb_decoder_rejects_oversized_input_without_parsing) {
  std::string huge = kHeader + std::string((size_t{64} << 20) + 1, '\0');
  CHECK(errorOf([&] { ai::decodeBinary(huge); }).find("larger than") != std::string::npos);
}

TEST(byb_roundtrip_is_stable_for_all_examples_at_every_level) {
  const char* files[] = {"hello.by", "arithmetic.by", "functions.by", "control_flow.by"};
  const OptLevel levels[] = {OptLevel::O0, OptLevel::O1, OptLevel::O2, OptLevel::O3, OptLevel::Os};
  for (const char* file : files) {
    for (OptLevel level : levels) {
      std::string encoded = compileToByb(example(file), level);
      CHECK_EQ(ai::encodeBinary(ai::decodeBinary(encoded)), encoded);
      CHECK_EQ(compileToByb(example(file), level), encoded);
    }
  }
}

TEST(source_semantics_do_not_depend_on_the_target) {
  ir::Module module = compileToIR(example("functions.by"), OptLevel::O2);
  std::string reference;
  for (const Target& target : allTargets()) {
    TargetConfig config{target, "", {}, "", ""};
    std::string text = emitLLVM(module, "functions.by", config);
    std::string triple = "target triple = \"" + target.llvmTriple + "\"";
    size_t at = text.find(triple);
    CHECK(at != std::string::npos);
    text.replace(at, triple.size(), "target triple = \"\"");
    if (reference.empty()) reference = text;
    CHECK_EQ(text, reference);
  }
  CHECK_EQ(ir::print(compileToIR(example("functions.by"), OptLevel::O2)), ir::print(module));
}

TEST(target_registry_resolves_and_rejects) {
  CHECK(findTarget("aarch64-linux-gnu") != nullptr);
  CHECK_EQ(resolveTarget("aarch64-linux-gnu").llvmTriple, "aarch64-unknown-linux-gnu");
  CHECK_EQ(resolveTarget("host").name, hostTarget().name);
  CHECK(errorOf([] { resolveTarget("sparc-solaris"); }).find("unsupported target 'sparc-solaris'") != std::string::npos);
  CHECK(errorOf([] { resolveTarget(""); }).find("unsupported target") != std::string::npos);
  for (const Target& t : allTargets()) {
    CHECK(t.verified != Verification::None);
    CHECK(t.pointerBits == 32 || t.pointerBits == 64);
  }
}

TEST(target_cpu_and_feature_validation) {
  CHECK(errorOf([] { validateCpu("a b"); }).find("invalid CPU name") != std::string::npos);
  CHECK(errorOf([] { validateCpu("x\"y"); }).find("invalid CPU name") != std::string::npos);
  CHECK(errorOf([] { parseFeatures("avx2"); }).find("invalid CPU feature") != std::string::npos);
  CHECK(errorOf([] { parseFeatures("+a,,+b"); }).find("invalid CPU feature") != std::string::npos);
  CHECK(errorOf([] { parseFeatures(""); }).find("empty CPU feature list") != std::string::npos);
  CHECK_EQ(featureString(parseFeatures("+avx2,-sse4a")), "+avx2,-sse4a");
}

TEST(llvm_emission_states_the_target_explicitly) {
  BuildOptions options;
  options.target = "aarch64-linux-gnu";
  options.cpu = "cortex-a76";
  options.features = "+sve";
  std::string text = compileToLLVM(example("hello.by"), "we\"ird\\name.by", options);
  CHECK(text.find("target triple = \"aarch64-unknown-linux-gnu\"") != std::string::npos);
  CHECK(text.find("source_filename = \"we\\22ird\\5Cname.by\"") != std::string::npos);
  CHECK(text.find("define i32 @main() #0 {") != std::string::npos);
  CHECK(text.find("attributes #0 = { \"target-cpu\"=\"cortex-a76\" \"target-features\"=\"+sve\" }") != std::string::npos);
  BuildOptions plain;
  plain.target = "riscv64-linux-gnu";
  std::string untuned = compileToLLVM(example("hello.by"), "hello.by", plain);
  CHECK(untuned.find("attributes") == std::string::npos);
}

TEST(objects_have_the_correct_format_for_every_registered_target) {
  if (skipUnlessClang()) return;
  for (const Target& target : allTargets()) {
    fs::path object = scratch() / ("obj_" + target.name + ".o");
    fs::remove(object);
    BuildOptions options;
    options.target = target.name;
    options.opt = OptLevel::O2;
    buildObject(example("arithmetic.by"), "arithmetic.by", object.string(), options);
    CHECK(fs::exists(object));
    expectObjectFormat(target, readBinary(object));
  }
}

TEST(object_generation_does_not_require_main) {
  if (skipUnlessClang()) return;
  fs::path object = scratch() / "lib.o";
  fs::remove(object);
  BuildOptions options;
  options.target = "aarch64-linux-gnu";
  buildObject("module lib;\nfn add(a: i32, b: i32) -> i32 { return a + b; }\n", "lib.by", object.string(), options);
  CHECK(fs::exists(object));
}

TEST(toolchain_failures_are_reported_and_leave_no_output) {
  fs::path out = scratch() / "failure_out";
  fs::remove(out);
  BuildOptions missingClang;
  missingClang.clang = (scratch() / "no-such-clang").string();
  CHECK(errorOf([&] { buildObject(example("hello.by"), "hello.by", out.string(), missingClang); }).find("cannot run") != std::string::npos);
  CHECK(!fs::exists(out));

  BuildOptions badTarget;
  badTarget.target = "riscv32-unknown-elf";
  CHECK(errorOf([&] { buildExecutable(example("hello.by"), "hello.by", out.string(), badTarget); }).find("unsupported target") != std::string::npos);
  CHECK(!fs::exists(out));

  BuildOptions badCpu;
  badCpu.cpu = "bad cpu";
  CHECK(errorOf([&] { buildObject(example("hello.by"), "hello.by", out.string(), badCpu); }).find("invalid CPU name") != std::string::npos);

  if (toolAvailable("clang")) {
    BuildOptions unknownCpu;
    unknownCpu.target = "aarch64-linux-gnu";
    unknownCpu.cpu = "no-such-cpu";
    CHECK(errorOf([&] { buildObject(example("hello.by"), "hello.by", out.string(), unknownCpu); }).find("does not support the requested CPU") != std::string::npos);
    CHECK(!fs::exists(out));

    BuildOptions missingLinker;
    missingLinker.linker = (scratch() / "no-such-ld").string();
    CHECK(errorOf([&] { buildExecutable(example("hello.by"), "hello.by", out.string(), missingLinker); }).find("linking for target") != std::string::npos);
    CHECK(!fs::exists(out));
  }
}

TEST(aarch64_linux_executable_runs_under_emulation) {
  const std::string linker = "aarch64-linux-gnu-ld";
  const fs::path sysroot = "/usr/aarch64-linux-gnu";
  if (!toolAvailable("clang") || !toolAvailable(linker) || !toolAvailable("qemu-aarch64") || !fs::exists(sysroot / "lib")) {
    harness::skip("needs clang, " + linker + ", qemu-aarch64 and " + sysroot.string());
    return;
  }
  struct Example {
    const char* file;
    int expected;
  };
  const Example examples[] = {{"hello.by", 42}, {"arithmetic.by", 54}, {"functions.by", 55}, {"control_flow.by", 56}};
  const OptLevel levels[] = {OptLevel::O0, OptLevel::O1, OptLevel::O2, OptLevel::O3, OptLevel::Os};
  for (const Example& e : examples) {
    for (OptLevel level : levels) {
      fs::path exe = scratch() / "a64_exe";
      fs::remove(exe);
      BuildOptions options;
      options.target = "aarch64-linux-gnu";
      options.linker = linker;
      options.opt = level;
      buildExecutable(example(e.file), e.file, exe.string(), options);
      std::string header = readBinary(exe).substr(0, 20);
      CHECK_EQ(le(header, 18, 2), 183u);
      ProcessResult run = runProcess({"qemu-aarch64", "-L", sysroot.string(), exe.string()});
      CHECK(run.started);
      CHECK_EQ(run.exitCode, e.expected);
    }
  }
}

TEST(cli_target_commands_and_help) {
  ProcessResult list = runProcess({BYLC_PATH, "--list-targets"});
  CHECK_EQ(list.exitCode, 0);
  for (const Target& t : allTargets()) CHECK(list.output.find(t.name) != std::string::npos);

  ProcessResult info = runProcess({BYLC_PATH, "--print-target-info", "--target", "aarch64-apple-darwin", "--cpu", "apple-m1", "--features", "+neon"});
  CHECK_EQ(info.exitCode, 0);
  CHECK(info.output.find("object format:    Mach-O") != std::string::npos);
  CHECK(info.output.find("cpu:              apple-m1") != std::string::npos);

  ProcessResult bad = runProcess({BYLC_PATH, "--print-target-info", "--target", "vax-vms"});
  CHECK_EQ(bad.exitCode, 2);

  ProcessResult help = runProcess({BYLC_PATH, "--help"});
  CHECK_EQ(help.exitCode, 0);
  for (const char* option : {"--emit-obj", "--emit-byai", "--emit-byb", "--emit-llvm", "--emit-ir", "--target", "--cpu", "--features", "--sysroot", "--linker", "--list-targets", "--print-target-info"}) {
    CHECK(help.output.find(option) != std::string::npos);
  }
}

TEST(cli_emit_obj_for_a_foreign_target) {
  if (skipUnlessClang()) return;
  fs::path object = scratch() / "cli_a64.o";
  fs::remove(object);
  ProcessResult r = runProcess({BYLC_PATH, (fs::path(BYLINK_EXAMPLES_DIR) / "hello.by").string(), "--target", "aarch64-linux-gnu", "--emit-obj", "-o", object.string()});
  CHECK_EQ(r.exitCode, 0);
  CHECK_EQ(le(readBinary(object), 18, 2), 183u);
}

TEST(cli_target_independent_outputs_do_not_need_a_toolchain) {
  fs::path byb = scratch() / "cli_hello.byb";
  fs::remove(byb);
  ProcessResult r = runProcess({BYLC_PATH, (fs::path(BYLINK_EXAMPLES_DIR) / "hello.by").string(), "--emit-byb", "--clang", (scratch() / "absent").string(), "-o", byb.string()});
  CHECK_EQ(r.exitCode, 0);
  CHECK_EQ(readBinary(byb), compileToByb(example("hello.by"), OptLevel::O2));
}
