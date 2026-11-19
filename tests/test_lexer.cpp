#include "harness.h"
#include "lexer/lexer.h"

using namespace bylink;

namespace {

std::string lexError(const std::string& source, Loc* loc = nullptr) {
  try {
    tokenize(source);
  } catch (const CompileError& e) {
    CHECK(e.category() == Category::Lexical);
    if (loc) *loc = e.loc();
    return e.what();
  }
  return "";
}

}

TEST(lexer_tokens_and_positions) {
  auto tokens = tokenize("fn main() -> i32 {\n  return 0x2A;\n}");
  CHECK_EQ(tokens.size(), 12u);
  CHECK(tokens[0].kind == Tok::KwFn);
  CHECK(tokens[1].kind == Tok::Ident);
  CHECK(tokens[4].kind == Tok::Arrow);
  CHECK(tokens[5].kind == Tok::TypeName);
  CHECK(*tokens[5].type == Type::I32);
  CHECK(tokens[8].kind == Tok::Int);
  CHECK_EQ(tokens[8].magnitude, 42u);
  CHECK_EQ(tokens[8].loc.line, 2u);
  CHECK_EQ(tokens[8].loc.column, 10u);
  CHECK(tokens.back().kind == Tok::End);
}

TEST(lexer_suffix_and_comments) {
  auto tokens = tokenize("// comment\n 7u8 // trailing\n");
  CHECK_EQ(tokens.size(), 2u);
  CHECK(*tokens[0].type == Type::U8);
  CHECK_EQ(tokens[0].loc.line, 2u);
}

TEST(lexer_operators) {
  auto tokens = tokenize("== != <= >= && || -> = < > & | ! ~");
  Tok expected[] = {Tok::EqEq, Tok::NotEq, Tok::Le, Tok::Ge, Tok::AmpAmp, Tok::PipePipe, Tok::Arrow,
                    Tok::Assign, Tok::Lt, Tok::Gt, Tok::Amp, Tok::Pipe, Tok::Bang, Tok::Tilde};
  for (size_t i = 0; i < 14; ++i) CHECK(tokens[i].kind == expected[i]);
}

TEST(lexer_errors) {
  Loc loc;
  CHECK(lexError("fn $", &loc).find("unexpected character") != std::string::npos);
  CHECK_EQ(loc.column, 4u);
  CHECK(lexError("1x").find("invalid integer literal suffix") != std::string::npos);
  CHECK(lexError("1bool").find("invalid integer literal suffix") != std::string::npos);
  CHECK(lexError("99999999999999999999").find("too large") != std::string::npos);
  CHECK(lexError("0x").find("no digits") != std::string::npos);
}
