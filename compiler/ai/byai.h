#pragma once

#include <string>

#include "ast/ast.h"

namespace bylink::ai {

inline constexpr int kByaiVersion = 1;

std::string encode(const ast::Module& module);
ast::Module decode(const std::string& text);

}
