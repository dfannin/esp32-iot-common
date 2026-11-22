// Host tests for IotTime and the uptime helpers (with stand-ins in fakes/): make -C tests/host

#include <cstdio>
#include <cstring>
#include <string>

#include "../../src/iot_time.h"

uint32_t fake_millis = 0;
int64_t fake_uptime_us = 0;
FakeSerial Serial;

// Fake SNTP: getLocalTime() fails until fake_time_valid, and "waits" its timeout
// by advancing fake_millis, as the real one blocks for up to ms.
static bool fake_time_valid = false;
static int getLocalTimeCalls = 0;
static std::string lastTz, lastServer;

void configTzTime(const char* tz, const char* server1, const char*, const char*) {
  lastTz = tz;
  lastServer = server1;
}

bool getLocalTime(struct tm* info, uint32_t ms) {
  getLocalTimeCalls++;
  if (!fake_time_valid) {
    fake_millis += ms;
    return false;
  }
  std::memset(info, 0, sizeof(*info));
  info->tm_year = 2026 - 1900;
  info->tm_mon = 10;  // November
  info->tm_mday = 22;
  info->tm_hour = 7;
  info->tm_min = 5;
  info->tm_sec = 9;
  return true;
}

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

static void reset() {
  fake_millis = 1000;
  fake_time_valid = false;
  getLocalTimeCalls = 0;
}

static void test_synced_at_boot() {
  reset();
  fake_time_valid = true;
  IotTime t;
  CHECK(t.begin("PST8PDT,M3.2.0,M11.1.0", "pool.ntp.org", 30000));
  CHECK(t.synced());
  CHECK(lastTz == "PST8PDT,M3.2.0,M11.1.0");
  CHECK(lastServer == "pool.ntp.org");
  CHECK(std::strcmp(t.timestamp().c_str(), "2026-11-22T07:05:09") == 0);
  CHECK(t.epoch() > 0);
}

static void test_not_synced_at_boot() {
  reset();
  IotTime t;
  CHECK(!t.begin("UTC0", "pool.ntp.org", 30000));  // gives up
  CHECK(!t.synced());
  CHECK(fake_millis - 1000 >= 30000);              // waited the limit
  CHECK(fake_millis - 1000 <= 31000);              // and not much longer
  CHECK(std::strcmp(t.timestamp().c_str(), "Time not synced") == 0);
  CHECK(std::strcmp(t.timestamp("not_synced").c_str(), "not_synced") == 0);

  // loop() does not wait while there is no time
  uint32_t before = fake_millis;
  t.loop();
  CHECK(fake_millis == before);
  CHECK(!t.synced());

  // The time arrives later: loop() picks it up, then stops asking
  fake_time_valid = true;
  t.loop();
  CHECK(t.synced());
  int calls = getLocalTimeCalls;
  t.loop();
  CHECK(getLocalTimeCalls == calls);
  CHECK(std::strcmp(t.timestamp("not_synced").c_str(), "2026-11-22T07:05:09") == 0);
}

static void test_timestamp_does_not_wait() {
  reset();
  IotTime t;
  uint32_t before = fake_millis;
  t.timestamp();  // the old device code waited up to 5 s here when not synced
  CHECK(fake_millis == before);
}

static void test_uptime() {
  fake_uptime_us = 0;
  CHECK(uptimeMillis() == 0);
  fake_uptime_us = 1999999;
  CHECK(uptimeMillis() == 1999);
  CHECK(uptimeSeconds() == 1);
  fake_uptime_us = 60LL * 24 * 3600 * 1000000;  // 60 days: past the 49.7-day millis() wrap
  CHECK(uptimeSeconds() == 60ULL * 24 * 3600);
  CHECK(uptimeMillis() == 60ULL * 24 * 3600 * 1000);
}

int main() {
  test_synced_at_boot();
  test_not_synced_at_boot();
  test_timestamp_does_not_wait();
  test_uptime();
  std::printf("iot_time: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
