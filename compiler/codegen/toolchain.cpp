#include "codegen/toolchain.h"

#include <filesystem>
#include <vector>

#include "common/diagnostic.h"
#include "common/process.h"

namespace bylink {

namespace {

[[noreturn]] void fail(const std::string& message) { throw CompileError(Category::Toolchain, Loc{}, message); }

std::string trimmed(std::string text) {
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) text.pop_back();
  return text;
}

std::vector<std::string> backendCommand(const std::string& clang, const TargetConfig& config, const std::string& optFlag,
                                        const std::string& llvmPath, const std::string& outputPath, Artifact artifact) {
  std::vector<std::string> argv = {clang, "--target=" + config.target.llvmTriple, "-x", "ir", "-" + optFlag};
  if (artifact == Artifact::Object) argv.push_back("-c");
  if (!config.sysroot.empty()) argv.push_back("--sysroot=" + config.sysroot);
  if (artifact == Artifact::Executable && !config.linker.empty()) {
    argv.push_back("--ld-path=" + config.linker);
  }
  argv.push_back(llvmPath);
  argv.push_back("-o");
  argv.push_back(outputPath);
  return argv;
}

bool backendIgnoredRequest(const std::string& output) {
  return output.find("is not a recognized processor") != std::string::npos || output.find("is not a recognized feature") != std::string::npos;
}

}

void runBackend(const std::string& clang, const TargetConfig& config, const std::string& optFlag, const std::string& llvmPath,
                const std::string& outputPath, Artifact artifact) {
  ProcessResult result = runProcess(backendCommand(clang, config, optFlag, llvmPath, outputPath, artifact));
  if (!result.started) fail("cannot run '" + clang + "'; install Clang or pass --clang PATH");
  std::string output = trimmed(result.output);
  if (result.exitCode == 0 && !backendIgnoredRequest(output)) return;
  std::error_code ignored;
  std::filesystem::remove(outputPath, ignored);
  if (result.exitCode == 0) fail("the LLVM backend does not support the requested CPU or features for target '" + config.target.name + "':\n" + output);
  std::string what = artifact == Artifact::Object ? "object generation" : "linking";
  std::string message = what + " for target '" + config.target.name + "' failed (exit code " + std::to_string(result.exitCode) + ")";
  if (!output.empty()) message += ":\n" + output;
  if (artifact == Artifact::Executable) {
    message += "\nhint: linking for a target needs that target's linker and C runtime; use --linker and --sysroot, or --emit-obj to stop after object generation";
  }
  fail(message);
}

}
