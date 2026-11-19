#pragma once

#include <optional>
#include <string>
#include <vector>

namespace bylink {

enum class Arch { X86_64, AArch64, Arm32, RiscV64, RiscV32 };
enum class Os { Linux, Windows, MacOS };
enum class ObjectFormat { Elf, Coff, MachO };

enum class Verification { None, Object, Linked, Emulated, Native };

struct Target {
  std::string name;
  std::string llvmTriple;
  Arch arch;
  Os os;
  std::string abi;
  ObjectFormat objectFormat;
  unsigned pointerBits;
  Verification verified;
};

struct TargetConfig {
  Target target;
  std::string cpu;
  std::vector<std::string> features;
  std::string sysroot;
  std::string linker;
};

const std::vector<Target>& allTargets();
const Target* findTarget(const std::string& name);
const Target& hostTarget();
Target resolveTarget(const std::string& name);

const char* archName(Arch arch);
const char* osName(Os os);
const char* objectFormatName(ObjectFormat format);
const char* verificationName(Verification level);
const char* endiannessName(const Target& target);

void validateCpu(const std::string& cpu);
std::vector<std::string> parseFeatures(const std::string& list);
std::string featureString(const std::vector<std::string>& features);

std::string describe(const TargetConfig& config);

}
