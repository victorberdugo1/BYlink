#include "harness.h"
#include "parser/parser.h"

using namespace bylink;

TEST(parser_rejects_deep_nesting) {
  const size_t n = 100000;
  const std::string parens = "module t;\nfn main() -> i32 { return " + std::string(n, '(') + "1" + std::string(n, ')') + "; }\n";
  std::string sum = "module t;\nfn main() -> i32 { return 1";
  for (size_t i = 0; i < n; ++i) sum += " + 1";
  sum += "; }\n";
  std::string unary = "module t;\nfn main() -> bool { return " + std::string(n, '!') + "true; }\n";
  std::string blocks = "module t;\nfn main() { " + std::string(n, '{') + std::string(n, '}') + " }\n";
  for (const std::string* source : std::initializer_list<const std::string*>{&parens, &sum, &unary, &blocks}) {
    try {
      parse(*source);
      CHECK(false);
    } catch (const CompileError& e) {
      CHECK(e.category() == Category::Syntax);
      CHECK(std::string(e.what()).find("nesting is too deep") != std::string::npos);
    }
  }
}

TEST(parser_accepts_moderate_nesting) {
  std::string source = "module t;\nfn main() -> i32 { return " + std::string(100, '(') + "1" + std::string(100, ')') + "; }\n";
  CHECK_EQ(parse(source).functions.size(), 1u);
}
