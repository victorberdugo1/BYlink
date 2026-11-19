#include "ai/byai.h"

#include <array>
#include <cctype>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace bylink::ai {

namespace {

using ast::BinaryKind;
using ast::Expr;
using ast::ExprKind;
using ast::ExprPtr;
using ast::Stmt;
using ast::StmtKind;
using ast::StmtList;
using ast::StmtPtr;
using ast::UnaryKind;

constexpr size_t kMaxNesting = 256;

struct BinaryName {
  BinaryKind kind;
  const char* name;
};

constexpr std::array<BinaryName, 16> kBinaryNames{{
    {BinaryKind::Add, "add"}, {BinaryKind::Sub, "sub"}, {BinaryKind::Mul, "mul"}, {BinaryKind::Div, "div"},
    {BinaryKind::Rem, "rem"}, {BinaryKind::BitAnd, "and"}, {BinaryKind::BitOr, "or"}, {BinaryKind::BitXor, "xor"},
    {BinaryKind::Eq, "eq"}, {BinaryKind::Ne, "ne"}, {BinaryKind::Lt, "lt"}, {BinaryKind::Le, "le"},
    {BinaryKind::Gt, "gt"}, {BinaryKind::Ge, "ge"}, {BinaryKind::LogAnd, "land"}, {BinaryKind::LogOr, "lor"},
}};

struct UnaryName {
  UnaryKind kind;
  const char* name;
};

constexpr std::array<UnaryName, 3> kUnaryNames{{{UnaryKind::Neg, "neg"}, {UnaryKind::Not, "not"}, {UnaryKind::BitNot, "bnot"}}};

const char* binaryName(BinaryKind kind) {
  for (const auto& b : kBinaryNames) {
    if (b.kind == kind) return b.name;
  }
  return "?";
}

const char* unaryName(UnaryKind kind) {
  for (const auto& u : kUnaryNames) {
    if (u.kind == kind) return u.name;
  }
  return "?";
}

const std::unordered_set<std::string>& reservedWords() {
  static const std::unordered_set<std::string> words = [] {
    std::unordered_set<std::string> set{"byai", "module", "const", "fn", "let", "mut", "set", "if", "else", "while", "scope",
                                        "end", "ret", "break", "continue", "call", "cast", "true", "false"};
    for (const auto& b : kBinaryNames) set.insert(b.name);
    for (const auto& u : kUnaryNames) set.insert(u.name);
    for (const char* t : {"i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "bool", "void"}) set.insert(t);
    return set;
  }();
  return words;
}

class Renamer {
 public:
  explicit Renamer(const ast::Module& module) {
    for (const auto& c : module.consts) collect(c.name);
    for (const auto& c : module.consts) collectExpr(*c.init);
    for (const auto& f : module.functions) {
      for (const auto& p : f.params) collect(p.name);
      collectList(f.body);
    }
    for (const std::string& name : used_) {
      if (!reservedWords().count(name)) continue;
      std::string candidate = name;
      do {
        candidate += "_";
      } while (used_.count(candidate) || reservedWords().count(candidate));
      map_[name] = candidate;
    }
  }

  const std::string& apply(const std::string& name) const {
    auto it = map_.find(name);
    return it == map_.end() ? name : it->second;
  }

 private:
  std::set<std::string> used_;
  std::unordered_map<std::string, std::string> map_;

  void collect(const std::string& name) { used_.insert(name); }

  void collectExpr(const Expr& e) {
    if (e.kind == ExprKind::Ident) collect(e.name);
    for (const auto& o : e.operands) collectExpr(*o);
  }

  void collectList(const StmtList& list) {
    for (const auto& s : list) {
      if (s->kind == StmtKind::Let || s->kind == StmtKind::Assign) collect(s->name);
      if (s->expr) collectExpr(*s->expr);
      collectList(s->body);
      collectList(s->elseBody);
    }
  }
};

class Printer {
 public:
  explicit Printer(const ast::Module& module) : module_(module), names_(module) {}

  std::string run() {
    out_ << "byai " << kByaiVersion << "\nmodule " << module_.name << "\n";
    for (const auto& c : module_.consts) {
      out_ << "const " << names_.apply(c.name) << " " << typeName(c.type) << " ";
      expr(*c.init);
      out_ << "\n";
    }
    for (const auto& f : module_.functions) function(f);
    return out_.str();
  }

 private:
  const ast::Module& module_;
  Renamer names_;
  std::ostringstream out_;

  void function(const ast::Function& f) {
    out_ << "fn " << f.name;
    for (const auto& p : f.params) out_ << " " << names_.apply(p.name) << ":" << typeName(p.type);
    if (f.ret != Type::Void) out_ << " > " << typeName(f.ret);
    out_ << "\n";
    list(f.body);
    out_ << "end\n";
  }

  void list(const StmtList& body) {
    for (const auto& s : body) stmt(*s);
  }

  void stmt(const Stmt& s) {
    switch (s.kind) {
      case StmtKind::Let:
        out_ << (s.isMutable ? "mut " : "let ") << names_.apply(s.name) << " " << typeName(s.declType) << " ";
        expr(*s.expr);
        out_ << "\n";
        break;
      case StmtKind::Assign:
        out_ << "set " << names_.apply(s.name) << " ";
        expr(*s.expr);
        out_ << "\n";
        break;
      case StmtKind::If:
        out_ << "if ";
        expr(*s.expr);
        out_ << "\n";
        list(s.body);
        if (s.hasElse) {
          out_ << "else\n";
          list(s.elseBody);
        }
        out_ << "end\n";
        break;
      case StmtKind::While:
        out_ << "while ";
        expr(*s.expr);
        out_ << "\n";
        list(s.body);
        out_ << "end\n";
        break;
      case StmtKind::Return:
        out_ << "ret";
        if (s.expr) {
          out_ << " ";
          expr(*s.expr);
        }
        out_ << "\n";
        break;
      case StmtKind::Break: out_ << "break\n"; break;
      case StmtKind::Continue: out_ << "continue\n"; break;
      case StmtKind::Call:
        expr(*s.expr);
        out_ << "\n";
        break;
      case StmtKind::Block:
        out_ << "scope\n";
        list(s.body);
        out_ << "end\n";
        break;
    }
  }

  void expr(const Expr& e) {
    switch (e.kind) {
      case ExprKind::IntLit:
        out_ << (e.negative ? "-" : "") << e.magnitude;
        if (e.suffix) out_ << typeName(*e.suffix);
        break;
      case ExprKind::BoolLit: out_ << (e.boolValue ? "true" : "false"); break;
      case ExprKind::Ident: out_ << names_.apply(e.name); break;
      case ExprKind::Unary:
        out_ << unaryName(e.unary) << " ";
        expr(*e.operands[0]);
        break;
      case ExprKind::Binary:
        out_ << binaryName(e.binary) << " ";
        expr(*e.operands[0]);
        out_ << " ";
        expr(*e.operands[1]);
        break;
      case ExprKind::Call:
        out_ << "call " << e.name;
        for (const auto& a : e.operands) {
          out_ << " ";
          expr(*a);
        }
        break;
      case ExprKind::Cast:
        out_ << "cast " << typeName(e.castTo) << " ";
        expr(*e.operands[0]);
        break;
    }
  }
};

struct Word {
  std::string text;
  uint32_t column;
};

struct Line {
  uint32_t number;
  std::vector<Word> words;
};

class Decoder {
 public:
  explicit Decoder(const std::string& text) { split(text); }

  ast::Module run() {
    ast::Module module;
    header();
    Line& m = next("expected 'module NAME'");
    expectWord(m, 0, "module");
    module.loc = locOf(m, 0);
    module.name = identifier(m, 1, "a module name");
    noMore(m, 2);
    while (pos_ < lines_.size()) {
      Line& line = lines_[pos_];
      if (line.words[0].text == "fn") function(module);
      else if (line.words[0].text == "const") constant(module);
      else fail(line, 0, "expected 'fn' or 'const' but found '" + line.words[0].text + "'");
    }
    return module;
  }

 private:
  std::vector<Line> lines_;
  size_t pos_ = 0;
  std::unordered_map<std::string, size_t> arity_;
  size_t depth_ = 0;

  [[noreturn]] static void fail(const Line& line, size_t word, const std::string& message) {
    uint32_t column = word < line.words.size() ? line.words[word].column : (line.words.empty() ? 1 : line.words.back().column + static_cast<uint32_t>(line.words.back().text.size()) + 1);
    throw CompileError(Category::Syntax, Loc{line.number, column}, message);
  }

  static Loc locOf(const Line& line, size_t word) {
    return Loc{line.number, word < line.words.size() ? line.words[word].column : 1};
  }

  void split(const std::string& text) {
    uint32_t number = 0;
    size_t start = 0;
    while (start <= text.size()) {
      size_t end = text.find('\n', start);
      if (end == std::string::npos) end = text.size();
      ++number;
      Line line{number, {}};
      size_t i = start;
      while (i < end) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == ' ' || c == '\t' || c == '\r') {
          ++i;
          continue;
        }
        if (c < 0x20 || c >= 0x7F) throw CompileError(Category::Lexical, Loc{number, static_cast<uint32_t>(i - start + 1)}, "invalid character");
        size_t j = i;
        while (j < end && text[j] != ' ' && text[j] != '\t' && text[j] != '\r') {
          unsigned char d = static_cast<unsigned char>(text[j]);
          if (d < 0x20 || d >= 0x7F) throw CompileError(Category::Lexical, Loc{number, static_cast<uint32_t>(j - start + 1)}, "invalid character");
          ++j;
        }
        line.words.push_back({text.substr(i, j - i), static_cast<uint32_t>(i - start + 1)});
        i = j;
      }
      if (!line.words.empty()) lines_.push_back(std::move(line));
      start = end + 1;
    }
    prescan();
  }

  void prescan() {
    for (const Line& line : lines_) {
      if (line.words[0].text != "fn" || line.words.size() < 2) continue;
      size_t params = 0;
      for (size_t i = 2; i < line.words.size() && line.words[i].text != ">"; ++i) ++params;
      arity_.emplace(line.words[1].text, params);
    }
  }

  Line& next(const std::string& message) {
    if (pos_ >= lines_.size()) {
      uint32_t number = lines_.empty() ? 1 : lines_.back().number;
      throw CompileError(Category::Syntax, Loc{number, 1}, message + " but found end of input");
    }
    return lines_[pos_++];
  }

  void header() {
    Line& line = next("expected 'byai 1'");
    if (line.words[0].text != "byai" || line.words.size() < 2) fail(line, 0, "expected the version header 'byai 1'");
    if (line.words[1].text != std::to_string(kByaiVersion)) fail(line, 1, "unsupported BYAI version '" + line.words[1].text + "' (this compiler reads version " + std::to_string(kByaiVersion) + ")");
    noMore(line, 2);
  }

  static void expectWord(const Line& line, size_t index, const char* word) {
    if (index >= line.words.size() || line.words[index].text != word) fail(line, index, std::string("expected '") + word + "'");
  }

  static void noMore(const Line& line, size_t index) {
    if (index < line.words.size()) fail(line, index, "unexpected '" + line.words[index].text + "'; check operand counts");
  }

  static bool validIdentifier(const std::string& s) {
    if (s.empty() || !(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
    for (char c : s) {
      if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    }
    return true;
  }

  static std::string identifier(const Line& line, size_t index, const char* what) {
    if (index >= line.words.size()) fail(line, index, std::string("expected ") + what);
    const std::string& s = line.words[index].text;
    if (!validIdentifier(s)) fail(line, index, std::string("expected ") + what + " but found '" + s + "'");
    return s;
  }

  static std::string freeName(const Line& line, size_t index, const char* what) {
    std::string s = identifier(line, index, what);
    if (reservedWords().count(s)) fail(line, index, "'" + s + "' is reserved and cannot be used as " + what);
    return s;
  }

  static Type typeAt(const Line& line, size_t index, bool allowVoid) {
    if (index >= line.words.size()) fail(line, index, "expected a type");
    auto t = typeFromName(line.words[index].text);
    if (!t) fail(line, index, "expected a type but found '" + line.words[index].text + "'");
    if (!allowVoid && *t == Type::Void) fail(line, index, "'void' is only allowed as a return type");
    return *t;
  }

  void constant(ast::Module& module) {
    Line& line = lines_[pos_++];
    ast::Const c;
    c.loc = locOf(line, 0);
    c.name = freeName(line, 1, "a constant name");
    c.type = typeAt(line, 2, false);
    size_t cursor = 3;
    c.init = expression(line, cursor);
    noMore(line, cursor);
    module.consts.push_back(std::move(c));
  }

  void function(ast::Module& module) {
    Line& line = lines_[pos_++];
    ast::Function f;
    f.loc = locOf(line, 0);
    f.name = identifier(line, 1, "a function name");
    size_t i = 2;
    for (; i < line.words.size() && line.words[i].text != ">"; ++i) {
      const std::string& w = line.words[i].text;
      size_t colon = w.find(':');
      if (colon == std::string::npos) fail(line, i, "expected a parameter 'name:type' but found '" + w + "'");
      ast::Param p;
      p.loc = locOf(line, i);
      p.name = w.substr(0, colon);
      if (!validIdentifier(p.name) || reservedWords().count(p.name)) fail(line, i, "invalid parameter name '" + p.name + "'");
      auto t = typeFromName(w.substr(colon + 1));
      if (!t || *t == Type::Void) fail(line, i, "expected a parameter type in '" + w + "'");
      p.type = *t;
      f.params.push_back(std::move(p));
    }
    if (i < line.words.size()) {
      f.ret = typeAt(line, i + 1, true);
      noMore(line, i + 2);
    }
    f.body = body(false).first;
    module.functions.push_back(std::move(f));
  }

  struct DepthGuard {
    DepthGuard(Decoder& d, const Line& line, size_t word) : decoder(d) {
      if (++decoder.depth_ > kMaxNesting) fail(line, word, "nesting is too deep (limit " + std::to_string(kMaxNesting) + ")");
    }
    ~DepthGuard() { --decoder.depth_; }
    DepthGuard(const DepthGuard&) = delete;
    DepthGuard& operator=(const DepthGuard&) = delete;
    Decoder& decoder;
  };

  std::pair<StmtList, bool> body(bool allowElse) {
    StmtList list;
    for (;;) {
      Line& line = next("expected 'end'");
      const std::string& head = line.words[0].text;
      if (head == "end") {
        noMore(line, 1);
        return {std::move(list), false};
      }
      if (head == "else") {
        if (!allowElse) fail(line, 0, "'else' without a matching 'if'");
        noMore(line, 1);
        return {std::move(list), true};
      }
      list.push_back(statement(line));
    }
  }

  StmtPtr makeStmt(StmtKind kind, const Line& line) {
    auto s = std::make_unique<Stmt>();
    s->kind = kind;
    s->loc = locOf(line, 0);
    return s;
  }

  StmtPtr statement(Line& line) {
    DepthGuard guard(*this, line, 0);
    const std::string& head = line.words[0].text;
    if (head == "let" || head == "mut") {
      StmtPtr s = makeStmt(StmtKind::Let, line);
      s->isMutable = head == "mut";
      s->name = freeName(line, 1, "a variable name");
      s->declType = typeAt(line, 2, false);
      size_t cursor = 3;
      s->expr = expression(line, cursor);
      noMore(line, cursor);
      return s;
    }
    if (head == "set") {
      StmtPtr s = makeStmt(StmtKind::Assign, line);
      s->name = freeName(line, 1, "a variable name");
      size_t cursor = 2;
      s->expr = expression(line, cursor);
      noMore(line, cursor);
      return s;
    }
    if (head == "if") {
      StmtPtr s = makeStmt(StmtKind::If, line);
      size_t cursor = 1;
      s->expr = expression(line, cursor);
      noMore(line, cursor);
      auto [thenBody, hasElse] = body(true);
      s->body = std::move(thenBody);
      if (hasElse) {
        s->hasElse = true;
        s->elseBody = body(false).first;
      }
      return s;
    }
    if (head == "while") {
      StmtPtr s = makeStmt(StmtKind::While, line);
      size_t cursor = 1;
      s->expr = expression(line, cursor);
      noMore(line, cursor);
      s->body = body(false).first;
      return s;
    }
    if (head == "scope") {
      StmtPtr s = makeStmt(StmtKind::Block, line);
      noMore(line, 1);
      s->body = body(false).first;
      return s;
    }
    if (head == "ret") {
      StmtPtr s = makeStmt(StmtKind::Return, line);
      size_t cursor = 1;
      if (cursor < line.words.size()) s->expr = expression(line, cursor);
      noMore(line, cursor);
      return s;
    }
    if (head == "break" || head == "continue") {
      StmtPtr s = makeStmt(head == "break" ? StmtKind::Break : StmtKind::Continue, line);
      noMore(line, 1);
      return s;
    }
    if (head == "call") {
      StmtPtr s = makeStmt(StmtKind::Call, line);
      size_t cursor = 0;
      s->expr = expression(line, cursor);
      noMore(line, cursor);
      return s;
    }
    fail(line, 0, "expected a statement but found '" + head + "'");
  }

  ExprPtr makeExpr(ExprKind kind, const Line& line, size_t word) {
    auto e = std::make_unique<Expr>();
    e->kind = kind;
    e->loc = locOf(line, word);
    return e;
  }

  ExprPtr expression(Line& line, size_t& cursor) {
    DepthGuard guard(*this, line, cursor);
    if (cursor >= line.words.size()) fail(line, cursor, "expected an operand but found end of line");
    size_t at = cursor;
    const std::string& w = line.words[cursor++].text;
    if (w == "true" || w == "false") {
      ExprPtr e = makeExpr(ExprKind::BoolLit, line, at);
      e->boolValue = w == "true";
      return e;
    }
    for (const auto& b : kBinaryNames) {
      if (w != b.name) continue;
      ExprPtr e = makeExpr(ExprKind::Binary, line, at);
      e->binary = b.kind;
      e->operands.push_back(expression(line, cursor));
      e->operands.push_back(expression(line, cursor));
      return e;
    }
    for (const auto& u : kUnaryNames) {
      if (w != u.name) continue;
      ExprPtr e = makeExpr(ExprKind::Unary, line, at);
      e->unary = u.kind;
      e->operands.push_back(expression(line, cursor));
      return e;
    }
    if (w == "cast") {
      ExprPtr e = makeExpr(ExprKind::Cast, line, at);
      e->castTo = typeAt(line, cursor, false);
      ++cursor;
      e->operands.push_back(expression(line, cursor));
      return e;
    }
    if (w == "call") {
      ExprPtr e = makeExpr(ExprKind::Call, line, at);
      e->name = identifier(line, cursor, "a function name");
      auto it = arity_.find(e->name);
      ++cursor;
      if (it == arity_.end()) fail(line, cursor - 1, "call to undefined function '" + e->name + "'");
      for (size_t i = 0; i < it->second; ++i) e->operands.push_back(expression(line, cursor));
      return e;
    }
    if (std::isdigit(static_cast<unsigned char>(w[0])) || (w[0] == '-' && w.size() > 1)) return literal(line, at);
    if (reservedWords().count(w) || !validIdentifier(w)) fail(line, at, "expected an operand but found '" + w + "'");
    ExprPtr e = makeExpr(ExprKind::Ident, line, at);
    e->name = w;
    return e;
  }

  ExprPtr literal(const Line& line, size_t at) {
    std::string w = line.words[at].text;
    ExprPtr e = makeExpr(ExprKind::IntLit, line, at);
    size_t i = 0;
    if (w[0] == '-') {
      e->negative = true;
      i = 1;
    }
    bool hex = w.size() > i + 1 && w[i] == '0' && (w[i + 1] == 'x' || w[i + 1] == 'X');
    if (hex) i += 2;
    size_t digitsStart = i;
    uint64_t value = 0;
    auto digit = [&](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (hex && c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (hex && c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };
    const uint64_t base = hex ? 16 : 10;
    for (; i < w.size() && digit(w[i]) >= 0; ++i) {
      uint64_t d = static_cast<uint64_t>(digit(w[i]));
      if (value > (UINT64_MAX - d) / base) fail(line, at, "integer literal '" + w + "' is too large");
      value = value * base + d;
    }
    if (i == digitsStart) fail(line, at, "invalid integer literal '" + w + "'");
    if (i < w.size()) {
      auto t = typeFromName(w.substr(i));
      if (!t || *t == Type::Void || *t == Type::Bool) fail(line, at, "invalid integer literal '" + w + "'");
      e->suffix = *t;
    }
    e->magnitude = value;
    e->name = std::to_string(value);
    return e;
  }
};

}

std::string encode(const ast::Module& module) { return Printer(module).run(); }

ast::Module decode(const std::string& text) { return Decoder(text).run(); }

}
