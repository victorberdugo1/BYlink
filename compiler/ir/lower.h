#pragma once

#include "ast/ast.h"
#include "ir/ir.h"

namespace bylink::ir {

Module lower(const ast::Module& module);

}
