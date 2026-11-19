#include "harness.h"

namespace harness {

namespace {
int failures = 0;
int skips = 0;
std::string lastReason;
}

int failures_count() { return failures; }
int skips_count() { return skips; }

void skip(const std::string& reason) {
  ++skips;
  lastReason = reason;
}

const std::string& lastSkipReason() { return lastReason; }

std::vector<TestCase>& registry() {
  static std::vector<TestCase> tests;
  return tests;
}

void reportFailure(const char* file, int line, const std::string& message) {
  ++failures;
  std::cerr << file << ":" << line << ": " << message << "\n";
}

}

int main() {
  int failedTests = 0;
  int skippedTests = 0;
  for (const auto& test : harness::registry()) {
    int failuresBefore = harness::failures_count();
    int skipsBefore = harness::skips_count();
    try {
      test.body();
    } catch (const std::exception& e) {
      harness::reportFailure(test.name.c_str(), 0, std::string("unexpected exception: ") + e.what());
    }
    bool failed = harness::failures_count() != failuresBefore;
    bool skipped = !failed && harness::skips_count() != skipsBefore;
    if (failed) ++failedTests;
    if (skipped) ++skippedTests;
    std::cout << (failed ? "FAIL " : skipped ? "SKIP " : "ok   ") << test.name;
    if (skipped) std::cout << ": " << harness::lastSkipReason();
    std::cout << "\n";
  }
  size_t passed = harness::registry().size() - static_cast<size_t>(failedTests) - static_cast<size_t>(skippedTests);
  std::cout << passed << " passed, " << failedTests << " failed, " << skippedTests << " skipped\n";
  return failedTests == 0 ? 0 : 1;
}
