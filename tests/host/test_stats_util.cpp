// Host unit tests for stats_util.h (no Arduino): make -C tests/host

#include <cstdio>

#include "../../src/stats_util.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                              \
  do {                                                           \
    checks++;                                                    \
    if (!(cond)) {                                               \
      failures++;                                                \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                            \
  } while (0)

static void test_percentile() {
  int none[1] = {0};
  CHECK(percentile(none, 0, 50) == 0);

  int one[1] = {-60};
  CHECK(percentile(one, 1, 10) == -60);

  int rssi[10] = {-50, -70, -60, -55, -65, -80, -45, -75, -58, -62};
  CHECK(percentile(rssi, 10, 0) == -80);    // minimum
  CHECK(percentile(rssi, 10, 100) == -45);  // maximum
  CHECK(percentile(rssi, 10, 10) == -80);   // nearest rank 1
  CHECK(percentile(rssi, 10, 50) == -62);   // nearest rank 5
  CHECK(percentile(rssi, 10, 95) == -45);   // nearest rank 10
  CHECK(rssi[0] == -80 && rssi[9] == -45);  // sorted in place

  float ping[4] = {12.5f, 3.0f, 40.0f, 7.5f};
  CHECK(percentile(ping, 4, 95) == 40.0f);
  CHECK(percentile(ping, 4, 50) == 7.5f);
}

static void test_should_roam() {
  CHECK(!shouldRoam(-75, 0, 8));    // nothing else heard
  CHECK(!shouldRoam(-75, -70, 8));  // 5 dB better: stay
  CHECK(shouldRoam(-75, -67, 8));   // exactly the margin: roam
  CHECK(shouldRoam(-75, -51, 8));   // much stronger: roam
  CHECK(!shouldRoam(-51, -75, 8));  // weaker: stay
}

int main() {
  test_percentile();
  test_should_roam();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
