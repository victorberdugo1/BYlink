#pragma once

#include <string>

#include "ast/ast.h"

namespace bylink {

ast::Module parse(const std::string& source);

}
