#include <fstream>
#include <stdexcept>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "common/diagnostic.h"
#include "common/version.h"
#include "driver/pipeline.h"
#include "opengl/bygl.h"

namespace {

constexpr int kExitOk = 0;
constexpr int kExitCompileError = 1;
constexpr int kExitUsage = 2;

enum class Emit { Executable, Object, LLVM, IR, Byai, Byb, Cpp };
enum class Info { None, ListTargets, PrintTargetInfo };

struct Cli {
  std::string input;
  std::optional<std::string> output;
  Emit emit = Emit::Executable;
  Info info = Info::None;
  bylink::BuildOptions build;
  std::string cxx;
};

void printUsage(std::ostream& out) {
  out << "usage: bylc <input.by|input.byai|input.byb|input.bygl> [options]\n"
         "       bylc --list-targets | --print-target-info [--target T] | --version | --help\n"
         "\n"
         "input is chosen by file extension: .byai is BYAI text, .byb is binary IR, .bygl is OpenGL scene source, anything else is BYlink source\n"
         "\n"
         "output (default: executable for the target):\n"
         "  -o FILE           output file\n"
         "  --emit-obj        target object file, no linking\n"
         "  --emit-llvm       LLVM IR text for the target\n"
         "  --emit-ir         BYlink IR text (target independent)\n"
         "  --emit-byai       BYAI text (needs a .by input)\n"
         "  --emit-byb        binary IR (target independent)\n"
         "  --emit-cpp        generated C++ for a .bygl OpenGL program\n"
         "  --cxx PATH        C++ compiler for .bygl programs (default: CXX, clang++, g++, c++)\n"
         "\n"
         "optimization:\n"
         "  -O0 -O1 -O2 -O3 -Os\n"
         "\n"
         "target (default: the host of this bylc build; an explicit target never falls back to the host):\n"
         "  --target NAME     one of the names printed by --list-targets\n"
         "  --cpu NAME        target CPU, passed to the LLVM backend as target-cpu\n"
         "  --features LIST   comma separated, such as +avx2,-sse4a, passed as target-features\n"
         "  --sysroot DIR     passed to Clang as --sysroot when linking or compiling\n"
         "  --linker TOOL     linker executable (name in PATH or path), passed to Clang as --ld-path\n"
         "  --clang PATH      Clang executable used for object generation and linking\n";
}

std::optional<bylink::OptLevel> parseOptLevel(const std::string& arg) {
  if (arg == "-O0") return bylink::OptLevel::O0;
  if (arg == "-O1") return bylink::OptLevel::O1;
  if (arg == "-O2") return bylink::OptLevel::O2;
  if (arg == "-O3") return bylink::OptLevel::O3;
  if (arg == "-Os") return bylink::OptLevel::Os;
  return std::nullopt;
}

std::string stem(const std::string& path) {
  size_t slash = path.find_last_of("/\\");
  std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  size_t dot = name.find_last_of('.');
  return dot == std::string::npos ? name : name.substr(0, dot);
}

bool targetDependent(Emit emit) { return emit == Emit::Executable || emit == Emit::Object || emit == Emit::LLVM; }

std::string defaultOutput(const Cli& cli) {
  bool windows = targetDependent(cli.emit) && bylink::resolveTarget(cli.build.target).os == bylink::Os::Windows;
  switch (cli.emit) {
    case Emit::Object: return stem(cli.input) + (windows ? ".obj" : ".o");
    case Emit::LLVM: return stem(cli.input) + ".ll";
    case Emit::IR: return stem(cli.input) + ".byir";
    case Emit::Byai: return stem(cli.input) + ".byai";
    case Emit::Byb: return stem(cli.input) + ".byb";
    case Emit::Cpp: return stem(cli.input) + ".cpp";
    case Emit::Executable: return stem(cli.input) + (windows ? ".exe" : "");
  }
  return stem(cli.input);
}

bool hasSuffix(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bylink::InputKind inputKind(const std::string& path) {
  if (hasSuffix(path, ".byai")) return bylink::InputKind::Byai;
  if (hasSuffix(path, ".byb")) return bylink::InputKind::Byb;
  if (hasSuffix(path, ".bygl")) return bylink::InputKind::Bygl;
  return bylink::InputKind::Source;
}

std::optional<Cli> parseArgs(const std::vector<std::string>& args) {
  Cli cli;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    auto value = [&]() -> const std::string& {
      if (i + 1 >= args.size()) throw std::invalid_argument("option '" + arg + "' needs a value");
      return args[++i];
    };
    if (arg == "-o") {
      cli.output = value();
    } else if (arg == "--target") {
      cli.build.target = value();
    } else if (arg == "--cpu") {
      cli.build.cpu = value();
    } else if (arg == "--features") {
      cli.build.features = value();
    } else if (arg == "--sysroot") {
      cli.build.sysroot = value();
    } else if (arg == "--linker") {
      cli.build.linker = value();
    } else if (arg == "--list-targets") {
      cli.info = Info::ListTargets;
    } else if (arg == "--print-target-info") {
      cli.info = Info::PrintTargetInfo;
    } else if (arg == "--emit-obj") {
      cli.emit = Emit::Object;
    } else if (arg == "--clang") {
      cli.build.clang = value();
    } else if (arg == "--emit-llvm") {
      cli.emit = Emit::LLVM;
    } else if (arg == "--emit-ir") {
      cli.emit = Emit::IR;
    } else if (arg == "--emit-byai") {
      cli.emit = Emit::Byai;
    } else if (arg == "--emit-byb") {
      cli.emit = Emit::Byb;
    } else if (arg == "--emit-cpp") {
      cli.emit = Emit::Cpp;
    } else if (arg == "--cxx") {
      cli.cxx = value();
    } else if (auto level = parseOptLevel(arg)) {
      cli.build.opt = *level;
    } else if (!arg.empty() && arg[0] == '-') {
      throw std::invalid_argument("unknown option '" + arg + "'");
    } else if (cli.input.empty()) {
      cli.input = arg;
    } else {
      throw std::invalid_argument("more than one input file given");
    }
  }
  if (cli.info != Info::None) return cli;
  if (cli.input.empty()) throw std::invalid_argument("no input file");
  cli.build.input = inputKind(cli.input);
  if (cli.build.input == bylink::InputKind::Bygl && cli.emit != Emit::Executable && cli.emit != Emit::Cpp)
    throw std::invalid_argument(".bygl supports executable output (default) or --emit-cpp");
  if (cli.emit == Emit::Cpp && cli.build.input != bylink::InputKind::Bygl)
    throw std::invalid_argument("--emit-cpp needs a .bygl input");
  if (cli.emit == Emit::Byai && cli.build.input != bylink::InputKind::Source) throw std::invalid_argument("--emit-byai needs a .by input");
  return cli;
}

std::string readFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read '" + path + "'");
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void writeFile(const std::string& path, const std::string& content) {
  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("cannot write '" + path + "'");
  out << content;
}

int printInfo(const Cli& cli) {
  if (cli.info == Info::ListTargets) {
    for (const bylink::Target& t : bylink::allTargets()) {
      std::cout << t.name << "  [" << bylink::verificationName(t.verified) << "]\n";
    }
    return kExitOk;
  }
  std::cout << bylink::describe(bylink::makeTargetConfig(cli.build));
  return kExitOk;
}

int run(const Cli& cli) {
  if (cli.info != Info::None) return printInfo(cli);
  std::string output = cli.output ? *cli.output : defaultOutput(cli);
  std::string source = readFile(cli.input);
  if (cli.build.input == bylink::InputKind::Bygl) {
    if (cli.emit == Emit::Cpp) writeFile(output, bylink::opengl::compileToCpp(source));
    else bylink::opengl::buildExecutable(source, cli.input, output, cli.cxx);
    return kExitOk;
  }
  switch (cli.emit) {
    case Emit::IR:
      writeFile(output, bylink::ir::print(bylink::compileToIR(source, cli.build.opt, cli.build.input)));
      break;
    case Emit::Byai:
      writeFile(output, bylink::compileToByai(source));
      break;
    case Emit::Byb:
      writeFile(output, bylink::compileToByb(source, cli.build.opt, cli.build.input));
      break;
    case Emit::Cpp:
      throw std::invalid_argument("--emit-cpp needs a .bygl input");
    case Emit::LLVM:
      writeFile(output, bylink::compileToLLVM(source, cli.input, cli.build));
      break;
    case Emit::Object:
      bylink::buildObject(source, cli.input, output, cli.build);
      break;
    case Emit::Executable:
      bylink::buildExecutable(source, cli.input, output, cli.build);
      break;
  }
  return kExitOk;
}

}

int main(int argc, char** argv) {
  std::vector<std::string> args(argv + 1, argv + argc);
  if (args.size() == 1 && args[0] == "--version") {
    std::cout << "bylc " << bylink::kCompilerVersion << " (language " << bylink::kLanguageVersion << ", IR "
              << bylink::kIRVersion << ")\n";
    return kExitOk;
  }
  if (args.size() == 1 && args[0] == "--help") {
    printUsage(std::cout);
    return kExitOk;
  }
  std::optional<Cli> cli;
  try {
    cli = parseArgs(args);
  } catch (const std::invalid_argument& e) {
    std::cerr << "bylc: " << e.what() << "\n";
    printUsage(std::cerr);
    return kExitUsage;
  }
  try {
    return run(*cli);
  } catch (const bylink::CompileError& e) {
    bool toolchain = e.category() == bylink::Category::Toolchain;
    std::cerr << e.format(toolchain ? "bylc" : cli->input) << "\n";
    return toolchain ? kExitUsage : kExitCompileError;
  } catch (const std::exception& e) {
    std::cerr << "bylc: " << e.what() << "\n";
    return kExitUsage;
  }
}
