#include "driver/pipeline.h"

#include <filesystem>
#include <fstream>

#include "codegen/llvm_emitter.h"
#include "ai/byai.h"
#include "ai/byb.h"
#include "codegen/toolchain.h"
#include "ir/verify.h"
#include "ir/lower.h"
#include "optimizer/optimizer.h"
#include "parser/parser.h"
#include "semantic/sema.h"

namespace bylink {

namespace {

class TempFile {
 public:
  explicit TempFile(std::filesystem::path path) : path_(std::move(path)) {}
  TempFile(const TempFile&) = delete;
  TempFile& operator=(const TempFile&) = delete;
  ~TempFile() {
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
  }
  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

}

TargetConfig makeTargetConfig(const BuildOptions& options) {
  TargetConfig config{resolveTarget(options.target), "", {}, options.sysroot, options.linker};
  if (!options.cpu.empty()) {
    validateCpu(options.cpu);
    config.cpu = options.cpu;
  }
  if (!options.features.empty()) config.features = parseFeatures(options.features);
  return config;
}

const char* optFlag(OptLevel level) {
  switch (level) {
    case OptLevel::O0: return "O0";
    case OptLevel::O1: return "O1";
    case OptLevel::O2: return "O2";
    case OptLevel::O3: return "O3";
    case OptLevel::Os: return "Os";
  }
  return "O2";
}

ir::Module compileToIR(const std::string& input, OptLevel level, InputKind kind) {
  ir::Module lowered;
  if (kind == InputKind::Byb) {
    lowered = ai::decodeBinary(input);
  } else {
    ast::Module module = kind == InputKind::Byai ? ai::decode(input) : parse(input);
    analyze(module);
    lowered = ir::lower(module);
    ir::verify(lowered);
  }
  if (level != OptLevel::O0) {
    ir::optimize(lowered);
    ir::verify(lowered);
  }
  return lowered;
}

std::string compileToByai(const std::string& source) {
  ast::Module module = parse(source);
  analyze(module);
  return ai::encode(module);
}

std::string compileToByb(const std::string& input, OptLevel level, InputKind kind) { return ai::encodeBinary(compileToIR(input, level, kind)); }

std::string compileToLLVM(const std::string& source, const std::string& sourceName, const BuildOptions& options) {
  TargetConfig config = makeTargetConfig(options);
  return emitLLVM(compileToIR(source, options.opt, options.input), sourceName, config);
}

namespace {

void buildArtifact(const std::string& source, const std::string& sourceName, const std::string& outputPath, const BuildOptions& options, Artifact artifact) {
  TargetConfig config = makeTargetConfig(options);
  ir::Module module = compileToIR(source, options.opt, options.input);
  if (artifact == Artifact::Executable && !module.find("main")) throw CompileError(Category::Semantic, Loc{}, "program has no 'main' function");
  TempFile llvmFile(outputPath + ".tmp.ll");
  std::ofstream out(llvmFile.path(), std::ios::binary);
  if (!out) throw CompileError(Category::Toolchain, Loc{}, "cannot write '" + llvmFile.path().string() + "'");
  out << emitLLVM(module, sourceName, config);
  out.close();
  runBackend(options.clang, config, optFlag(options.opt), llvmFile.path().string(), outputPath, artifact);
}

}

void buildObject(const std::string& source, const std::string& sourceName, const std::string& outputPath, const BuildOptions& options) {
  buildArtifact(source, sourceName, outputPath, options, Artifact::Object);
}

void buildExecutable(const std::string& source, const std::string& sourceName, const std::string& outputPath, const BuildOptions& options) {
  buildArtifact(source, sourceName, outputPath, options, Artifact::Executable);
}

}
