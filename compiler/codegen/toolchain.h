#pragma once

#include <string>

#include "target/target.h"

namespace bylink {

enum class Artifact { Object, Executable };

void runBackend(const std::string& clang, const TargetConfig& config, const std::string& optFlag, const std::string& llvmPath,
                const std::string& outputPath, Artifact artifact);

}
