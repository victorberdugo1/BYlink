#pragma once

#include <string>

#include "ir/ir.h"
#include "target/target.h"

namespace bylink {

enum class OptLevel { O0, O1, O2, O3, Os };

const char* optFlag(OptLevel level);

enum class InputKind { Source, Byai, Byb, Bygl };

struct BuildOptions {
  InputKind input = InputKind::Source;
  OptLevel opt = OptLevel::O2;
  std::string clang = "clang";
  std::string target = "host";
  std::string cpu;
  std::string features;
  std::string sysroot;
  std::string linker;
};

TargetConfig makeTargetConfig(const BuildOptions& options);

ir::Module compileToIR(const std::string& input, OptLevel level, InputKind kind = InputKind::Source);
std::string compileToByai(const std::string& source);
std::string compileToByb(const std::string& input, OptLevel level, InputKind kind = InputKind::Source);
std::string compileToLLVM(const std::string& source, const std::string& sourceName, const BuildOptions& options);
void buildObject(const std::string& source, const std::string& sourceName, const std::string& outputPath, const BuildOptions& options);
void buildExecutable(const std::string& source, const std::string& sourceName, const std::string& outputPath, const BuildOptions& options);

}
