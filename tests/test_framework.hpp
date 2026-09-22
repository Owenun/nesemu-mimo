#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace ne_test {

struct TestCase {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> cases;
  return cases;
}

inline int& failures() {
  static int f = 0;
  return f;
}

struct Registrar {
  Registrar(const char* name, std::function<void()> fn) {
    registry().push_back(TestCase{name, std::move(fn)});
  }
};

#define NE_TEST(name)                                                                 \
  static void ne_test_##name();                                                       \
  static ::ne_test::Registrar ne_reg_##name(#name, ne_test_##name);                   \
  static void ne_test_##name()

#define NE_CHECK(cond)                                                                \
  do {                                                                                \
    if (!(cond)) {                                                                    \
      std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                   \
      ::ne_test::failures()++;                                                        \
    }                                                                                 \
  } while (0)

#define NE_CHECK_EQ(a, b)                                                             \
  do {                                                                                \
    const auto _a = (a);                                                              \
    const auto _b = (b);                                                              \
    if (!(_a == _b)) {                                                                \
      std::printf("  FAIL %s:%d  %s == %s  (%lld vs %lld)\n", __FILE__, __LINE__, #a,  \
                  #b, static_cast<long long>(_a), static_cast<long long>(_b));         \
      ::ne_test::failures()++;                                                        \
    }                                                                                 \
  } while (0)

inline int run_all() {
  int ran = 0;
  for (auto& t : registry()) {
    std::printf("[ RUN ] %s\n", t.name.c_str());
    const int before = failures();
    t.fn();
    if (failures() == before) {
      std::printf("[ OK  ] %s\n", t.name.c_str());
    } else {
      std::printf("[FAIL ] %s\n", t.name.c_str());
    }
    ++ran;
  }
  std::printf("%d tests, %d failures\n", ran, failures());
  return failures() == 0 ? 0 : 1;
}

}  // namespace ne_test
