#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

#include "ai/byai.h"
#include "ai/byb.h"
#include "driver/pipeline.h"

using namespace bylink;
namespace fs = std::filesystem;

namespace {

struct Program {
  std::string name;
  std::string source;
};

std::string readFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::vector<Program> loadPrograms(const std::vector<fs::path>& dirs) {
  std::vector<Program> programs;
  for (const fs::path& dir : dirs) {
    for (const auto& entry : fs::directory_iterator(dir)) {
      if (entry.path().extension() == ".by") programs.push_back({entry.path().stem().string(), readFile(entry.path())});
    }
  }
  std::sort(programs.begin(), programs.end(), [](const Program& a, const Program& b) { return a.name < b.name; });
  return programs;
}

using Clock = std::chrono::steady_clock;

template <typename F>
double microsPerCall(F&& fn, int iterations) {
  auto start = Clock::now();
  for (int i = 0; i < iterations; ++i) fn();
  return std::chrono::duration<double, std::micro>(Clock::now() - start).count() / iterations;
}

int runTiming(const std::vector<Program>& programs) {
  const int iterations = 300;
  std::cout << "representation,bytes,parse_validate_to_ir_us\n";
  size_t sourceBytes = 0, byaiBytes = 0, irBytes = 0, bybBytes = 0;
  double sourceUs = 0, byaiUs = 0, bybUs = 0;
  for (const Program& p : programs) {
    std::string byai = compileToByai(p.source);
    std::string byb = compileToByb(p.source, OptLevel::O0);
    std::string irText = ir::print(compileToIR(p.source, OptLevel::O0));
    sourceBytes += p.source.size();
    byaiBytes += byai.size();
    bybBytes += byb.size();
    irBytes += irText.size();
    sourceUs += microsPerCall([&] { compileToIR(p.source, OptLevel::O0, InputKind::Source); }, iterations);
    byaiUs += microsPerCall([&] { compileToIR(byai, OptLevel::O0, InputKind::Byai); }, iterations);
    bybUs += microsPerCall([&] { compileToIR(byb, OptLevel::O0, InputKind::Byb); }, iterations);
  }
  std::cout << std::fixed << std::setprecision(1);
  std::cout << "source," << sourceBytes << "," << sourceUs << "\n";
  std::cout << "byai," << byaiBytes << "," << byaiUs << "\n";
  std::cout << "ir_text," << irBytes << ",n/a\n";
  std::cout << "byb," << bybBytes << "," << bybUs << "\n";
  return 0;
}

std::vector<std::pair<size_t, size_t>> lexemes(const std::string& text, bool lineOriented) {
  std::vector<std::pair<size_t, size_t>> spans;
  size_t i = 0;
  auto isWord = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
  while (i < text.size()) {
    char c = text[i];
    if (c == ' ' || c == '\t' || c == '\r' || (c == '\n' && !lineOriented)) {
      ++i;
      continue;
    }
    size_t j = i + 1;
    if (c == '\n') {
    } else if (lineOriented) {
      while (j < text.size() && text[j] != ' ' && text[j] != '\n' && text[j] != '\t' && text[j] != '\r') ++j;
    } else if (isWord(c)) {
      while (j < text.size() && isWord(text[j])) ++j;
    } else if (j < text.size()) {
      std::string two = text.substr(i, 2);
      if (two == "==" || two == "!=" || two == "<=" || two == ">=" || two == "&&" || two == "||" || two == "->") ++j;
    }
    spans.emplace_back(i, j);
    i = j;
  }
  return spans;
}

std::string mutate(const std::string& text, const std::vector<std::pair<size_t, size_t>>& spans, std::mt19937_64& rng) {
  auto pick = [&](size_t n) { return static_cast<size_t>(rng() % n); };
  auto textOf = [&](size_t k) { return text.substr(spans[k].first, spans[k].second - spans[k].first); };
  size_t k = pick(spans.size());
  switch (pick(4)) {
    case 0: return text.substr(0, spans[k].first) + text.substr(spans[k].second);
    case 1: return text.substr(0, spans[k].second) + " " + textOf(k) + text.substr(spans[k].second);
    case 2: {
      if (k + 1 >= spans.size()) return text.substr(0, spans[k].first) + text.substr(spans[k].second);
      return text.substr(0, spans[k].first) + textOf(k + 1) + text.substr(spans[k].second, spans[k + 1].first - spans[k].second) + textOf(k) + text.substr(spans[k + 1].second);
    }
    default: return text.substr(0, spans[k].first) + textOf(pick(spans.size())) + text.substr(spans[k].second);
  }
}

struct Outcome {
  size_t rejected = 0, identical = 0, different = 0;
};

Outcome mutationStudy(const std::vector<Program>& programs, InputKind kind, uint64_t seed, int perProgram) {
  Outcome total;
  for (const Program& p : programs) {
    std::string text = kind == InputKind::Source ? p.source : compileToByai(p.source);
    std::string baseline = ir::print(compileToIR(p.source, OptLevel::O0));
    auto spans = lexemes(text, kind == InputKind::Byai);
    std::mt19937_64 rng(seed);
    for (int n = 0; n < perProgram; ++n) {
      std::string mutated = mutate(text, spans, rng);
      try {
        std::string ir = ir::print(compileToIR(mutated, OptLevel::O0, kind));
        (ir == baseline ? total.identical : total.different)++;
      } catch (const CompileError&) {
        ++total.rejected;
      }
    }
  }
  return total;
}

int runMutation(const std::vector<Program>& programs, uint64_t seed, int perProgram) {
  std::cout << "representation,mutations,rejected,accepted_identical_ir,accepted_different_ir,silent_change_rate_percent\n";
  for (auto [name, kind] : {std::pair<const char*, InputKind>{"source", InputKind::Source}, {"byai", InputKind::Byai}}) {
    Outcome o = mutationStudy(programs, kind, seed, perProgram);
    size_t total = o.rejected + o.identical + o.different;
    std::cout << name << "," << total << "," << o.rejected << "," << o.identical << "," << o.different << "," << std::fixed << std::setprecision(2)
              << 100.0 * static_cast<double>(o.different) / static_cast<double>(total) << "\n";
  }
  return 0;
}

}

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "usage: bylink_ai_bench time|mutate CORPUS_DIR [EXTRA_DIR] [seed count]\n";
    return 2;
  }
  std::string mode = argv[1];
  std::vector<fs::path> dirs{argv[2]};
  if (argc > 3 && fs::is_directory(argv[3])) dirs.push_back(argv[3]);
  std::vector<Program> programs = loadPrograms(dirs);
  if (mode == "time") return runTiming(programs);
  uint64_t seed = argc > 5 ? std::stoull(argv[4]) : 1;
  int count = argc > 5 ? std::stoi(argv[5]) : 2000;
  if (mode == "mutate") return runMutation(programs, seed, count);
  return 2;
}
