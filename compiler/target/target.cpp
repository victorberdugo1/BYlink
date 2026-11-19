#include "target/target.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "common/diagnostic.h"

namespace bylink {

namespace {

[[noreturn]] void fail(const std::string& message) { throw CompileError(Category::Toolchain, Loc{}, message); }

const std::vector<Target> kTargets = {
    {"x86_64-linux-gnu", "x86_64-unknown-linux-gnu", Arch::X86_64, Os::Linux, "gnu", ObjectFormat::Elf, 64, Verification::Native},
    {"aarch64-linux-gnu", "aarch64-unknown-linux-gnu", Arch::AArch64, Os::Linux, "gnu", ObjectFormat::Elf, 64, Verification::Emulated},
    {"x86_64-windows-gnu", "x86_64-w64-windows-gnu", Arch::X86_64, Os::Windows, "gnu", ObjectFormat::Coff, 64, Verification::Object},
    {"aarch64-windows-gnu", "aarch64-w64-windows-gnu", Arch::AArch64, Os::Windows, "gnu", ObjectFormat::Coff, 64, Verification::Object},
    {"x86_64-apple-darwin", "x86_64-apple-darwin", Arch::X86_64, Os::MacOS, "darwin", ObjectFormat::MachO, 64, Verification::Object},
    {"aarch64-apple-darwin", "aarch64-apple-darwin", Arch::AArch64, Os::MacOS, "darwin", ObjectFormat::MachO, 64, Verification::Object},
    {"armv7-linux-gnueabihf", "armv7-unknown-linux-gnueabihf", Arch::Arm32, Os::Linux, "gnueabihf", ObjectFormat::Elf, 32, Verification::Object},
    {"riscv64-linux-gnu", "riscv64-unknown-linux-gnu", Arch::RiscV64, Os::Linux, "gnu", ObjectFormat::Elf, 64, Verification::Object},
    {"riscv32-linux-gnu", "riscv32-unknown-linux-gnu", Arch::RiscV32, Os::Linux, "gnu", ObjectFormat::Elf, 32, Verification::Object},
};

bool isNameChar(char c, const char* extra) {
  return std::isalnum(static_cast<unsigned char>(c)) || std::string_view(extra).find(c) != std::string_view::npos;
}

}

const std::vector<Target>& allTargets() { return kTargets; }

const Target* findTarget(const std::string& name) {
  for (const Target& t : kTargets) {
    if (t.name == name) return &t;
  }
  return nullptr;
}

const Target& hostTarget() {
  std::optional<Arch> arch;
  std::optional<Os> os;
#if defined(__x86_64__) || defined(_M_X64)
  arch = Arch::X86_64;
#elif defined(__aarch64__) || defined(_M_ARM64)
  arch = Arch::AArch64;
#endif
#if defined(__linux__)
  os = Os::Linux;
#elif defined(_WIN32)
  os = Os::Windows;
#elif defined(__APPLE__)
  os = Os::MacOS;
#endif
  if (arch && os) {
    for (const Target& t : kTargets) {
      if (t.arch == *arch && t.os == *os) return t;
    }
  }
  fail("this host is not in the target registry; pass --target explicitly");
}

Target resolveTarget(const std::string& name) {
  if (name == "host") return hostTarget();
  const Target* target = findTarget(name);
  if (!target) fail("unsupported target '" + name + "'; run 'bylc --list-targets' for the supported targets");
  return *target;
}

const char* archName(Arch arch) {
  switch (arch) {
    case Arch::X86_64: return "x86_64";
    case Arch::AArch64: return "aarch64";
    case Arch::Arm32: return "arm32";
    case Arch::RiscV64: return "riscv64";
    case Arch::RiscV32: return "riscv32";
  }
  return "?";
}

const char* osName(Os os) {
  switch (os) {
    case Os::Linux: return "linux";
    case Os::Windows: return "windows";
    case Os::MacOS: return "macos";
  }
  return "?";
}

const char* objectFormatName(ObjectFormat format) {
  switch (format) {
    case ObjectFormat::Elf: return "ELF";
    case ObjectFormat::Coff: return "COFF";
    case ObjectFormat::MachO: return "Mach-O";
  }
  return "?";
}

const char* verificationName(Verification level) {
  switch (level) {
    case Verification::None: return "none";
    case Verification::Object: return "object generation";
    case Verification::Linked: return "linked executable";
    case Verification::Emulated: return "executed under emulation";
    case Verification::Native: return "executed natively";
  }
  return "?";
}

const char* endiannessName(const Target&) { return "little"; }

void validateCpu(const std::string& cpu) {
  bool ok = !cpu.empty() && std::all_of(cpu.begin(), cpu.end(), [](char c) { return isNameChar(c, "_.-"); });
  if (!ok) fail("invalid CPU name '" + cpu + "'");
}

std::vector<std::string> parseFeatures(const std::string& list) {
  std::vector<std::string> features;
  std::stringstream stream(list);
  std::string item;
  while (std::getline(stream, item, ',')) {
    bool ok = item.size() >= 2 && (item[0] == '+' || item[0] == '-') &&
              std::all_of(item.begin() + 1, item.end(), [](char c) { return isNameChar(c, "_.-"); });
    if (!ok) fail("invalid CPU feature '" + item + "'; expected a comma separated list such as +avx2,-sse4a");
    features.push_back(item);
  }
  if (features.empty()) fail("empty CPU feature list");
  return features;
}

std::string featureString(const std::vector<std::string>& features) {
  std::string out;
  for (size_t i = 0; i < features.size(); ++i) {
    if (i != 0) out += ',';
    out += features[i];
  }
  return out;
}

std::string describe(const TargetConfig& config) {
  const Target& t = config.target;
  std::ostringstream out;
  out << "name:             " << t.name << "\n"
      << "llvm triple:      " << t.llvmTriple << "\n"
      << "architecture:     " << archName(t.arch) << "\n"
      << "operating system: " << osName(t.os) << "\n"
      << "abi:              " << t.abi << "\n"
      << "object format:    " << objectFormatName(t.objectFormat) << "\n"
      << "pointer width:    " << t.pointerBits << "\n"
      << "endianness:       " << endiannessName(t) << "\n"
      << "verified level:   " << verificationName(t.verified) << "\n"
      << "cpu:              " << (config.cpu.empty() ? "(toolchain default)" : config.cpu) << "\n"
      << "features:         " << (config.features.empty() ? "(toolchain default)" : featureString(config.features)) << "\n"
      << "sysroot:          " << (config.sysroot.empty() ? "(toolchain default)" : config.sysroot) << "\n"
      << "linker:           " << (config.linker.empty() ? "(toolchain default)" : config.linker) << "\n";
  return out.str();
}

}
