// Host tests for IotOta (with stand-ins in fakes/): make -C tests/host

#include <cstdio>

#include <ArduinoOTA.h>
#include <ESPmDNS.h>

#include "../../src/iot_ota.h"

uint32_t fake_millis = 0;
FakeSerial Serial;
FakeArduinoOTA ArduinoOTA;
FakeMDNS MDNS;

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

static int startCalls = 0;
static void onStart() { startCalls++; }

static void test_begin() {
  IotOta ota;
  ota.begin("backyard00", "secret", onStart);
  CHECK(ota.mdnsStarted());
  CHECK(MDNS.hostname == "backyard00");
  CHECK(MDNS.services.size() == 2);
  CHECK(MDNS.services[0] == "http/tcp/80");
  CHECK(MDNS.services[1] == "ota/tcp/3232");
  CHECK(ArduinoOTA.hostname == "backyard00");
  CHECK(ArduinoOTA.password == "secret");
  CHECK(ArduinoOTA.beginCalls == 1);

  // The device's function runs when an update starts
  ArduinoOTA.start();
  CHECK(startCalls == 1);

  // Progress with a small total does not divide by zero (the old code did total / 100)
  ArduinoOTA.progress(10, 50);
  ArduinoOTA.progress(0, 0);
  ArduinoOTA.error(OTA_AUTH_ERROR);
  ArduinoOTA.end();

  ota.loop();
  ota.loop();
  CHECK(ArduinoOTA.handleCalls == 2);
}

static void test_no_start_function_and_mdns_failure() {
  ArduinoOTA = FakeArduinoOTA();
  MDNS = FakeMDNS();
  MDNS.ok = false;
  IotOta ota;
  ota.begin("shed00", "pw");
  CHECK(!ota.mdnsStarted());
  CHECK(MDNS.services.empty());     // no services without mDNS
  CHECK(ArduinoOTA.beginCalls == 1);  // OTA still starts (by IP address)
  ArduinoOTA.start();               // no device function: nothing to call
  CHECK(startCalls == 1);
}

int main() {
  test_begin();
  test_no_start_function_and_mdns_failure();
  std::printf("\niot_ota: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
