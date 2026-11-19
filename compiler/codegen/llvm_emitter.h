#pragma once

#include <string>

#include "ir/ir.h"
#include "target/target.h"

namespace bylink {

std::string emitLLVM(const ir::Module& module, const std::string& sourceName, const TargetConfig& config);

}
