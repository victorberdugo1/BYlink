#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ir/ir.h"

namespace bylink::ai {

inline constexpr uint8_t kBybVersion = 0;

std::string encodeBinary(const ir::Module& module);
ir::Module decodeBinary(const std::string& bytes);

}
