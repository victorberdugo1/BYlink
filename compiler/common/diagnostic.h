#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace bylink {

struct Loc {
  uint32_t line = 0;
  uint32_t column = 0;
};

enum class Category { Lexical, Syntax, Type, Semantic, Toolchain };

const char* categoryName(Category category);

class CompileError : public std::runtime_error {
 public:
  CompileError(Category category, Loc loc, const std::string& message);

  Category category() const { return category_; }
  Loc loc() const { return loc_; }
  std::string format(const std::string& origin) const;

 private:
  Category category_;
  Loc loc_;
};

}
