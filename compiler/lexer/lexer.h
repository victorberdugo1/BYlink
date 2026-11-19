#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "common/diagnostic.h"
#include "types/types.h"

namespace bylink {

enum class Tok {
  End, Ident, Int, TypeName,
  KwModule, KwFn, KwLet, KwMut, KwConst, KwIf, KwElse, KwWhile,
  KwReturn, KwBreak, KwContinue, KwAs, KwTrue, KwFalse,
  LParen, RParen, LBrace, RBrace, Comma, Colon, Semi, Arrow, Assign,
  EqEq, NotEq, Lt, Le, Gt, Ge,
  Plus, Minus, Star, Slash, Percent, Amp, Pipe, Caret, Tilde, Bang, AmpAmp, PipePipe
};

struct Token {
  Tok kind = Tok::End;
  std::string text;
  Loc loc;
  uint64_t magnitude = 0;
  std::optional<Type> type;
};

std::vector<Token> tokenize(const std::string& source);

}
