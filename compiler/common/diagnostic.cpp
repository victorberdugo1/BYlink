#include "common/diagnostic.h"

namespace bylink {

const char* categoryName(Category category) {
  switch (category) {
    case Category::Lexical: return "lexical";
    case Category::Syntax: return "syntax";
    case Category::Type: return "type";
    case Category::Semantic: return "semantic";
    case Category::Toolchain: return "toolchain";
  }
  return "unknown";
}

CompileError::CompileError(Category category, Loc loc, const std::string& message)
    : std::runtime_error(message), category_(category), loc_(loc) {}

std::string CompileError::format(const std::string& origin) const {
  std::string out = origin;
  if (loc_.line != 0) {
    out += ":" + std::to_string(loc_.line) + ":" + std::to_string(loc_.column);
  }
  out += ": error[";
  out += categoryName(category_);
  out += "]: ";
  out += what();
  return out;
}

}
