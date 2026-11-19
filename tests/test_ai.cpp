#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "ai/byai.h"
#include "ai/byb.h"
#include "driver/pipeline.h"
#include "harness.h"
#include "ir/verify.h"
#include "parser/parser.h"

#ifndef _WIN32
#include <sys/wait.h>
#endif

using namespace bylink;
namespace fs = std::filesystem;

namespace {

const char* kCorpus[] = {
    "module a;\nfn main() -> i32 { return 7; }\n",
    "module a;\nconst K: i32 = 3 * 4;\nfn main() -> i32 { return K + 1; }\n",
    "module a;\nfn sq(n: i32) -> i32 { return n * n; }\nfn main() -> i32 {\n  let mut t: i32 = 0;\n  let mut i: i32 = 1;\n  while i <= 10 { t = t + sq(i); i = i + 1; }\n  return t % 256;\n}\n",
    "module a;\nfn f(x: u8, y: i64) -> i64 { return (x as i64) + y - -3; }\nfn main() -> i32 { return f(200, 5) as i32 & 0x7F; }\n",
    "module a;\nfn main() -> i32 {\n  let a: bool = true;\n  let b: bool = false;\n  if a && !b || b { return 1; } else if b { return 2; } else { return 3; }\n}\n",
    "module a;\nfn g() { }\nfn main() -> i32 { g(); { let x: i32 = 5; g(); } return ~0 + 1; }\n",
    "module a;\nfn main() -> i32 {\n  let mut i: i32 = 0;\n  while true { i = i + 1; if i == 9 { break; } if i < 3 { continue; } }\n  return i;\n}\n",
};

std::string irOf(const std::string& input, OptLevel level, InputKind kind) { return ir::print(compileToIR(input, level, kind)); }

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

CompileError decodeError(const std::string& text) {
  try {
    ai::decode(text);
  } catch (const CompileError& e) {
    return e;
  }
  throw std::runtime_error("expected a decode error");
}

int exitCodeOf(const std::string& input, InputKind kind, const std::string& name) {
  fs::path dir = fs::temp_directory_path() / "bylink-tests";
  fs::create_directories(dir);
  fs::path exe = dir / name;
  fs::remove(exe);
  BuildOptions options;
  options.input = kind;
  buildExecutable(input, name, exe.string(), options);
  int raw = std::system(("\"" + exe.string() + "\"").c_str());
#ifdef _WIN32
  return raw;
#else
  return WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
#endif
}

}

TEST(ai_roundtrip_preserves_ir_at_every_level) {
  for (const char* source : kCorpus) {
    std::string byai = compileToByai(source);
    for (OptLevel level : {OptLevel::O0, OptLevel::O2}) {
      std::string expected = irOf(source, level, InputKind::Source);
      CHECK_EQ(irOf(byai, level, InputKind::Byai), expected);
      std::string byb = compileToByb(source, level);
      CHECK_EQ(irOf(byb, level, InputKind::Byb), expected);
    }
  }
}

TEST(ai_text_encoding_is_canonical_and_stable) {
  for (const char* source : kCorpus) {
    std::string byai = compileToByai(source);
    CHECK_EQ(ai::encode(ai::decode(byai)), byai);
    CHECK_EQ(compileToByai(source), byai);
  }
}

TEST(ai_binary_encoding_is_canonical_and_stable) {
  for (const char* source : kCorpus) {
    std::string byb = compileToByb(source, OptLevel::O0);
    CHECK_EQ(ai::encodeBinary(ai::decodeBinary(byb)), byb);
    CHECK_EQ(compileToByb(source, OptLevel::O0), byb);
  }
}

TEST(ai_identifiers_that_collide_with_keywords_are_renamed) {
  const char* source =
      "module a;\nconst end: i32 = 2;\nfn add(and: i32, end_: i32) -> i32 { let mut or: i32 = and; or = or + end + end_; return or; }\n"
      "fn main() -> i32 { return add(1, 3); }\n";
  std::string byai = compileToByai(source);
  CHECK_EQ(irOf(byai, OptLevel::O0, InputKind::Byai), irOf(source, OptLevel::O0, InputKind::Source));
  CHECK_EQ(exitCodeOf(byai, InputKind::Byai, "ai_collide"), 6);
}

TEST(ai_native_execution_from_all_representations) {
  const char* source = kCorpus[2];
  CHECK_EQ(exitCodeOf(source, InputKind::Source, "ai_src"), 129);
  CHECK_EQ(exitCodeOf(compileToByai(source), InputKind::Byai, "ai_text"), 129);
  CHECK_EQ(exitCodeOf(compileToByb(source, OptLevel::O2), InputKind::Byb, "ai_bin"), 129);
}

TEST(ai_decoder_reports_positioned_errors) {
  CompileError version = decodeError("byai 2\nmodule a\n");
  CHECK(version.category() == Category::Syntax);
  CHECK_EQ(version.loc().line, 1u);
  CHECK(contains(version.what(), "unsupported BYAI version"));

  CompileError missing = decodeError("byai 1\nmodule a\nfn main > i32\n  ret add 1\nend\n");
  CHECK_EQ(missing.loc().line, 4u);
  CHECK(contains(missing.what(), "expected an operand"));

  CompileError extra = decodeError("byai 1\nmodule a\nfn main > i32\n  ret add 1i32 2i32 3i32\nend\n");
  CHECK_EQ(extra.loc().line, 4u);
  CHECK(contains(extra.what(), "check operand counts"));

  CompileError unknown = decodeError("byai 1\nmodule a\nfn main > i32\n  ret call nope\nend\n");
  CHECK(contains(unknown.what(), "undefined function"));

  CompileError reserved = decodeError("byai 1\nmodule a\nfn main > i32\n  let add i32 1\n  ret 0\nend\n");
  CHECK(contains(reserved.what(), "reserved"));

  CompileError open = decodeError("byai 1\nmodule a\nfn main > i32\n  ret 0\n");
  CHECK(contains(open.what(), "expected 'end'"));

  CompileError elseOnly = decodeError("byai 1\nmodule a\nfn main > i32\n  else\n  ret 0\nend\n");
  CHECK(contains(elseOnly.what(), "'else' without"));

  CompileError nonAscii = decodeError("byai 1\nmodule a\nfn main > i32\n  ret \xC3\xA9\nend\n");
  CHECK(nonAscii.category() == Category::Lexical);
}

TEST(ai_semantic_errors_still_come_from_the_shared_checker) {
  try {
    compileToIR("byai 1\nmodule a\nfn main > i32\n  ret true\nend\n", OptLevel::O0, InputKind::Byai);
    CHECK(false);
  } catch (const CompileError& e) {
    CHECK(e.category() == Category::Type);
    CHECK_EQ(e.loc().line, 4u);
  }
}

TEST(ai_decoder_rejects_deep_nesting) {
  std::string text = "byai 1\nmodule a\nfn main > i32\n  ret ";
  for (int i = 0; i < 100000; ++i) text += "neg ";
  text += "1\nend\n";
  CHECK(contains(decodeError(text).what(), "nesting is too deep"));
}

TEST(ai_binary_decoder_survives_truncation_and_bit_flips) {
  for (const char* source : kCorpus) {
    std::string good = compileToByb(source, OptLevel::O0);
    for (size_t n = 0; n < good.size(); ++n) {
      try {
        ai::decodeBinary(good.substr(0, n));
        CHECK(false);
      } catch (const CompileError&) {
      }
    }
    for (size_t i = 0; i < good.size(); ++i) {
      for (int bit = 0; bit < 8; ++bit) {
        std::string bad = good;
        bad[i] = static_cast<char>(bad[i] ^ (1 << bit));
        try {
          ir::Module m = ai::decodeBinary(bad);
          CHECK_EQ(ai::encodeBinary(m), bad);
        } catch (const CompileError&) {
        }
      }
    }
  }
}

TEST(ai_binary_decoder_rejects_bad_headers_and_trailing_data) {
  std::string good = compileToByb(kCorpus[0], OptLevel::O0);
  const std::string badInputs[] = {"", "BYIX", std::string("BYIR\x07", 5), good + "x", std::string("BYIR\x00\x01", 6) + "a\xFF\xFF\xFF\xFF\x0F"};
  for (const std::string& bad : badInputs) {
    try {
      ai::decodeBinary(bad);
      CHECK(false);
    } catch (const CompileError&) {
    }
  }
}

TEST(ir_verifier_rejects_malformed_modules) {
  ir::Module good = compileToIR(kCorpus[2], OptLevel::O0);
  ir::verify(good);

  ir::Module typeMismatch = good;
  for (ir::Instr& in : typeMismatch.functions.back().blocks[0].instrs) {
    if (in.op == ir::Opcode::Store) in.type = Type::I64;
  }
  bool threw = false;
  try {
    ir::verify(typeMismatch);
  } catch (const CompileError&) {
    threw = true;
  }
  CHECK(threw);

  ir::Module noTerminator = good;
  noTerminator.functions[0].blocks[0].instrs.pop_back();
  threw = false;
  try {
    ir::verify(noTerminator);
  } catch (const CompileError&) {
    threw = true;
  }
  CHECK(threw);

  ir::Module useBeforeDef = good;
  ir::Function& f = useBeforeDef.functions.back();
  f.blocks[0].instrs.insert(f.blocks[0].instrs.begin(), f.blocks[0].instrs.back());
  threw = false;
  try {
    ir::verify(useBeforeDef);
  } catch (const CompileError&) {
    threw = true;
  }
  CHECK(threw);
}
