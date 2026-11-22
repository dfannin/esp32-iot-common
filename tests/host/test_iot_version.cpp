// Host tests for iot_version.h: make -C tests/host

#include <cstdio>
#include <cstring>

#include "../../src/iot_version.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                               \
  do {                                                            \
    checks++;                                                     \
    if (!(cond)) {                                                \
      failures++;                                                 \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                             \
  } while (0)

// Evaluated by the compiler: the fallback costs nothing at run time.
static_assert(iotMonthFromDate("Nov 22 2026") == 11, "month");
static_assert(iotVersionValid("261122-1530"), "valid");

static void test_local_version() {
  CHECK(std::strcmp(iotLocalVersion("Nov 22 2026", "15:30:12").text, "261122-1530-local") == 0);
  CHECK(std::strcmp(iotLocalVersion("Jan  2 2027", "07:05:59").text, "270102-0705-local") == 0);  // day with a space
  CHECK(std::strcmp(iotLocalVersion("Dec 31 2099", "23:59:00").text, "991231-2359-local") == 0);
  CHECK(iotMonthFromDate("Jan 1 2026") == 1);
  CHECK(iotMonthFromDate("Xyz 1 2026") == 0);
}

static void test_fallback_macros() {
  // No version.h in this test: the -local fallback from the compiler's build time
  CHECK(iotVersionValid(FW_VERSION));
  CHECK(std::strstr(FW_VERSION, "-local") != nullptr);
  CHECK(std::strlen(FW_VERSION) == 17);
  CHECK(std::strcmp(FW_BUILD, "") == 0);
}

static void test_valid() {
  CHECK(iotVersionValid("261122-1530"));
  CHECK(iotVersionValid("261122-1530-dirty"));
  CHECK(iotVersionValid("261122-1530-local"));
  CHECK(!iotVersionValid("4.9.0"));
  CHECK(!iotVersionValid("2.8.0-m3a"));
  CHECK(!iotVersionValid("2611221530"));          // no dash
  CHECK(!iotVersionValid("261122-153"));          // too short
  CHECK(!iotVersionValid("261122-1530-m3a"));     // other suffixes are not allowed
  CHECK(!iotVersionValid("261122-1530-dirtyx"));
  CHECK(!iotVersionValid("261122-1530-dirt"));
  CHECK(!iotVersionValid("261122-1530-dirty-local"));
  CHECK(!iotVersionValid(""));
}

int main() {
  test_local_version();
  test_fallback_macros();
  test_valid();
  std::printf("iot_version: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
