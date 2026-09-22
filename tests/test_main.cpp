#include "test_framework.hpp"

#include <cstdio>

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  std::printf("tests starting, count=%zu\n", ne_test::registry().size());
  return ne_test::run_all();
}
