// Host tests for retry_schedule.h and IotMqtt (with stand-ins in fakes/): make -C tests/host

#include <cstdio>
#include <cstring>

#include <WiFi.h>

#include "../../src/iot_mqtt.h"
#include "../../src/retry_schedule.h"

uint32_t fake_millis = 0;
int64_t fake_uptime_us = 0;
FakeSerial Serial;
FakeWiFi WiFi;

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

static void test_schedule() {
  RetrySchedule r = {15000, 300000, 60000};
  CHECK(r.intervalMs(1000, 0) == 15000);       // outage just started
  CHECK(r.intervalMs(299999, 0) == 15000);
  CHECK(r.intervalMs(300000, 0) == 60000);     // after 5 minutes
  CHECK(r.due(1000, 0, 0, false));             // first attempt of an outage: at once
  CHECK(!r.due(10000, 0, 1000, true));         // 9 s after an attempt: not yet
  CHECK(r.due(16000, 0, 1000, true));          // 15 s after: due
  CHECK(!r.due(350000, 0, 320000, true));      // slow phase: 30 s is not enough
  CHECK(r.due(380000, 0, 320000, true));       // 60 s is
  // millis() wraps after 49.7 days
  uint32_t down = 0xFFFFF000u;                 // 4 s before the wrap
  CHECK(r.intervalMs(1000, down) == 15000);    // 5 s into the outage
  CHECK(r.due(20000, down, 2000, true));       // 18 s after the last attempt
}

static void test_state_text() {
  CHECK(std::strcmp(mqttStateText(5), "not authorized") == 0);
  CHECK(std::strcmp(mqttStateText(4), "bad user or password") == 0);
  CHECK(std::strcmp(mqttStateText(-2), "connect failed") == 0);
  CHECK(std::strcmp(mqttStateText(42), "unknown state") == 0);
}

static int connectedCalls = 0;
static void onConnected() { connectedCalls++; }
static void callback(char*, uint8_t*, unsigned int) {}

static IotMqttConfig config() {
  IotMqttConfig c;
  c.server = "broker";
  c.port = 8883;
  c.clientId = "backyard00";
  c.user = "backyard00";
  c.password = "secret";
  c.caCert = "-----BEGIN CERTIFICATE-----";
  c.willTopic = "sensor/backyard/status";
  return c;
}

static void test_begin_connects_and_configures() {
  fake_millis = 1000;
  WiFi.s = WL_CONNECTED;
  connectedCalls = 0;
  IotMqtt mqtt;
  mqtt.client().nextConnectOk = true;
  IotMqttConfig c = config();
  {
    String temporary = "{\"state\":\"OFFLINE\"}";
    c.willPayload = temporary.c_str();
    mqtt.begin(c, callback, onConnected);
  }  // the caller's string is gone: the will must have been copied
  PubSubClient& client = mqtt.client();
  CHECK(client.connectCalls == 1);
  CHECK(client.lastWillTopic == "sensor/backyard/status");
  CHECK(client.lastWill == "{\"state\":\"OFFLINE\"}");
  CHECK(client.port == 8883 && client.buffer == 2048 && client.keepalive == 60);
  CHECK(client.hasCallback);
  CHECK(client.tls->ca != nullptr);             // certificate checked: TLS only
  CHECK(client.tls->timeout == 10);             // not the 120 s default
  CHECK(mqtt.connects() == 1 && connectedCalls == 1);
  CHECK(mqtt.downForS() == 0);
  mqtt.loop();
  CHECK(client.connectCalls == 1 && client.loopCalls == 1);  // connected: no new attempt
}

static void test_fast_then_slow_retries() {
  fake_millis = 1000;
  fake_uptime_us = 5000000;
  WiFi.s = WL_CONNECTED;
  IotMqtt mqtt;
  PubSubClient& client = mqtt.client();
  client.nextConnectOk = false;
  client.tls->tlsCode = -9984;
  client.tls->tlsText = "X509 - Certificate verification failed";
  mqtt.begin(config(), callback, onConnected);
  CHECK(client.connectCalls == 1);
  CHECK(mqtt.failedAttempts() == 1);
  CHECK(std::strstr(mqtt.lastError(), "connect failed (rc=-2)") != nullptr);
  CHECK(std::strstr(mqtt.lastError(), "Certificate verification failed") != nullptr);
  CHECK(mqtt.lastErrorUptimeS() == 5);

  fake_millis = 10000;  mqtt.loop();  CHECK(client.connectCalls == 1);  // 9 s: wait
  fake_millis = 16000;  mqtt.loop();  CHECK(client.connectCalls == 2);  // 15 s: retry
  fake_millis = 31000;  mqtt.loop();  CHECK(client.connectCalls == 3);
  // After 5 minutes of outage: every 60 s
  fake_millis = 301000; mqtt.loop();  int n = client.connectCalls;
  fake_millis = 331000; mqtt.loop();  CHECK(client.connectCalls == n);
  fake_millis = 361001; mqtt.loop();  CHECK(client.connectCalls == n + 1);
  CHECK(mqtt.downForS() == 360);

  // Success resets the failure count and runs the device's connect function
  connectedCalls = 0;
  client.nextConnectOk = true;
  fake_millis = 421001; mqtt.loop();
  CHECK(client.connected() && mqtt.failedAttempts() == 0 && connectedCalls == 1);
}

static void test_lost_connection_retries_at_once() {
  fake_millis = 1000;
  WiFi.s = WL_CONNECTED;
  IotMqtt mqtt;
  PubSubClient& client = mqtt.client();
  client.nextConnectOk = true;
  mqtt.begin(config(), callback, onConnected);
  fake_millis = 900000;  // long after the last attempt
  client.up = false;     // the broker went away
  client.nextConnectOk = false;
  mqtt.loop();
  CHECK(client.connectCalls == 2);              // retried at once
  fake_millis = 905000; mqtt.loop(); CHECK(client.connectCalls == 2);  // then fast schedule
  fake_millis = 915000; mqtt.loop(); CHECK(client.connectCalls == 3);
}

static void test_no_attempt_without_wifi_and_retry_now() {
  fake_millis = 1000;
  WiFi.s = WL_DISCONNECTED;
  IotMqtt mqtt;
  PubSubClient& client = mqtt.client();
  client.nextConnectOk = false;
  mqtt.begin(config(), callback, onConnected);
  CHECK(client.connectCalls == 0);              // no WiFi: no attempt
  fake_millis = 100000; mqtt.loop(); CHECK(client.connectCalls == 0);
  WiFi.s = WL_CONNECTED;
  mqtt.loop(); CHECK(client.connectCalls == 1); // WiFi back: first attempt at once
  fake_millis = 101000; mqtt.retryNow(); mqtt.loop();
  CHECK(client.connectCalls == 2);              // e.g. after roaming
}

int main() {
  test_schedule();
  test_state_text();
  test_begin_connects_and_configures();
  test_fast_then_slow_retries();
  test_lost_connection_retries_at_once();
  test_no_attempt_without_wifi_and_retry_now();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
