#include "lexer/lexer.h"

#include <cctype>
#include <unordered_map>

namespace bylink {

namespace {

const std::unordered_map<std::string, Tok>& keywords() {
  static const std::unordered_map<std::string, Tok> table{
      {"module", Tok::KwModule}, {"fn", Tok::KwFn},         {"let", Tok::KwLet},
      {"mut", Tok::KwMut},       {"const", Tok::KwConst},   {"if", Tok::KwIf},
      {"else", Tok::KwElse},     {"while", Tok::KwWhile},   {"return", Tok::KwReturn},
      {"break", Tok::KwBreak},   {"continue", Tok::KwContinue}, {"as", Tok::KwAs},
      {"true", Tok::KwTrue},     {"false", Tok::KwFalse},
  };
  return table;
}

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentPart(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

int digitValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

class Lexer {
 public:
  explicit Lexer(const std::string& source) : src_(source) {}

  std::vector<Token> run() {
    std::vector<Token> tokens;
    for (;;) {
      skipTrivia();
      if (pos_ >= src_.size()) break;
      tokens.push_back(next());
    }
    Token end;
    end.kind = Tok::End;
    end.loc = here();
    end.text = "end of file";
    tokens.push_back(end);
    return tokens;
  }

 private:
  const std::string& src_;
  size_t pos_ = 0;
  uint32_t line_ = 1;
  uint32_t column_ = 1;

  Loc here() const { return Loc{line_, column_}; }
  char peek(size_t ahead = 0) const { return pos_ + ahead < src_.size() ? src_[pos_ + ahead] : '\0'; }

  char advance() {
    char c = src_[pos_++];
    if (c == '\n') {
      ++line_;
      column_ = 1;
    } else {
      ++column_;
    }
    return c;
  }

  [[noreturn]] void fail(Loc loc, const std::string& message) const {
    throw CompileError(Category::Lexical, loc, message);
  }

  void skipTrivia() {
    for (;;) {
      char c = peek();
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        advance();
      } else if (c == '/' && peek(1) == '/') {
        while (pos_ < src_.size() && peek() != '\n') advance();
      } else {
        return;
      }
    }
  }

  Token make(Tok kind, Loc loc, const std::string& text) const {
    Token t;
    t.kind = kind;
    t.loc = loc;
    t.text = text;
    return t;
  }

  Token next() {
    Loc loc = here();
    char c = peek();
    if (isIdentStart(c)) return word(loc);
    if (std::isdigit(static_cast<unsigned char>(c))) return number(loc);
    return punctuation(loc);
  }

  Token word(Loc loc) {
    std::string text;
    while (isIdentPart(peek())) text += advance();
    if (auto type = typeFromName(text)) {
      Token t = make(Tok::TypeName, loc, text);
      t.type = type;
      return t;
    }
    auto it = keywords().find(text);
    return make(it != keywords().end() ? it->second : Tok::Ident, loc, text);
  }

  Token number(Loc loc) {
    std::string text;
    unsigned base = 10;
    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
      text += advance();
      text += advance();
      base = 16;
      if (digitValue(peek()) < 0) fail(loc, "hexadecimal literal has no digits");
    }
    uint64_t value = 0;
    bool overflow = false;
    for (;;) {
      int digit = digitValue(peek());
      if (digit < 0 || static_cast<unsigned>(digit) >= base) break;
      if (value > (~uint64_t{0} - static_cast<uint64_t>(digit)) / base) overflow = true;
      value = value * base + static_cast<uint64_t>(digit);
      text += advance();
    }
    if (overflow) fail(loc, "integer literal '" + text + "' is too large");
    Token t = make(Tok::Int, loc, text);
    t.magnitude = value;
    if (isIdentStart(peek())) {
      Loc suffixLoc = here();
      std::string suffix;
      while (isIdentPart(peek())) suffix += advance();
      auto type = typeFromName(suffix);
      if (!type || !isInteger(*type)) fail(suffixLoc, "invalid integer literal suffix '" + suffix + "'");
      t.type = type;
      t.text += suffix;
    }
    return t;
  }

  Token punctuation(Loc loc) {
    char c = advance();
    auto two = [&](char second, Tok doubled, Tok single, const char* doubledText, const char* singleText) {
      if (peek() == second) {
        advance();
        return make(doubled, loc, doubledText);
      }
      return make(single, loc, singleText);
    };
    switch (c) {
      case '(': return make(Tok::LParen, loc, "(");
      case ')': return make(Tok::RParen, loc, ")");
      case '{': return make(Tok::LBrace, loc, "{");
      case '}': return make(Tok::RBrace, loc, "}");
      case ',': return make(Tok::Comma, loc, ",");
      case ':': return make(Tok::Colon, loc, ":");
      case ';': return make(Tok::Semi, loc, ";");
      case '+': return make(Tok::Plus, loc, "+");
      case '*': return make(Tok::Star, loc, "*");
      case '/': return make(Tok::Slash, loc, "/");
      case '%': return make(Tok::Percent, loc, "%");
      case '^': return make(Tok::Caret, loc, "^");
      case '~': return make(Tok::Tilde, loc, "~");
      case '-': return two('>', Tok::Arrow, Tok::Minus, "->", "-");
      case '=': return two('=', Tok::EqEq, Tok::Assign, "==", "=");
      case '!': return two('=', Tok::NotEq, Tok::Bang, "!=", "!");
      case '<': return two('=', Tok::Le, Tok::Lt, "<=", "<");
      case '>': return two('=', Tok::Ge, Tok::Gt, ">=", ">");
      case '&': return two('&', Tok::AmpAmp, Tok::Amp, "&&", "&");
      case '|': return two('|', Tok::PipePipe, Tok::Pipe, "||", "|");
      default: fail(loc, std::string("unexpected character '") + c + "'");
    }
  }
};

}

std::vector<Token> tokenize(const std::string& source) { return Lexer(source).run(); }

}
