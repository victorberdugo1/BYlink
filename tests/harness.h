#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace harness {

struct TestCase {
  std::string name;
  std::function<void()> body;
};

std::vector<TestCase>& registry();
void reportFailure(const char* file, int line, const std::string& message);
int failures_count();
void skip(const std::string& reason);
int skips_count();
const std::string& lastSkipReason();

struct Registrar {
  Registrar(const char* name, std::function<void()> body) { registry().push_back({name, std::move(body)}); }
};

}

#define TEST(name)                                         \
  static void test_##name();                               \
  static harness::Registrar registrar_##name(#name, test_##name); \
  static void test_##name()

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) harness::reportFailure(__FILE__, __LINE__, "CHECK(" #cond ")"); \
  } while (0)

#define CHECK_EQ(actual, expected)                                                         \
  do {                                                                                     \
    auto a_ = (actual);                                                                    \
    auto e_ = (expected);                                                                  \
    if (!(a_ == e_)) {                                                                     \
      harness::reportFailure(__FILE__, __LINE__, std::string("CHECK_EQ(" #actual ", " #expected ") got '") + \
                                                     harness::str(a_) + "' expected '" + harness::str(e_) + "'"); \
    }                                                                                      \
  } while (0)

namespace harness {

inline std::string str(const std::string& s) { return s; }
inline std::string str(const char* s) { return s; }
template <typename T>
std::string str(const T& v) { return std::to_string(static_cast<long long>(v)); }

}
