// Host tests for iot_status (with stand-ins in fakes/): make -C tests/host

#include <cstdio>
#include <cstring>
#include <string>

#include <WiFi.h>

#include "../../src/iot_status.h"

uint32_t fake_millis = 0;
int64_t fake_uptime_us = 0;
uint32_t fake_cpu_mhz = 80;
FakeSerial Serial;
FakeWiFi WiFi;
FakeEsp ESP;

// SNTP stand-ins for IotTime (time not synced in these tests)
void configTzTime(const char*, const char*, const char*, const char*) {}
bool getLocalTime(struct tm*, uint32_t) { return false; }

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

static bool has(const String& s, const char* part) { return std::strstr(s.c_str(), part) != nullptr; }

static IotStatusConfig config() {
  IotStatusConfig c;
  c.name = "backyard00";
  c.firmware = "4.9.0";
  c.board = "ESP32 DEVKITV1 (ESP-WROOM-32)";
  c.ssid = "home";
  c.mqttServer = "broker.local";
  c.mqttPort = 8883;
  c.mqttUser = "backyard00";
  c.statusTopic = "sensor/backyard/status";
  c.telemetryTopic = "telemetry/backyard/device";
  return c;
}

static void test_helpers() {
  CHECK(jsonEscape("a\"b\\c\nd") == "a\\\"b\\\\c\\nd");
  CHECK(jsonEscape(String('\x01')) == "\\u0001");
  CHECK(jsonEscape("25\xC2\xB0" "C") == "25\xC2\xB0" "C");  // UTF-8 unchanged
  CHECK(htmlEscape("<a href='x'>&\"</a>") == "&lt;a href=&#39;x&#39;&gt;&amp;&quot;&lt;/a&gt;");
  CHECK(u64ToString(0) == "0");
  CHECK(u64ToString(18446744073709551615ULL) == "18446744073709551615");
  CHECK(formatUptime(0) == "0s");
  CHECK(formatUptime(59) == "59s");
  CHECK(formatUptime(3600) == "1h 0s");
  CHECK(formatUptime(3 * 86400 + 4 * 3600 + 5 * 60 + 6) == "3d 4h 5m 6s");
  CHECK(resetReasonText(ESP_RST_BROWNOUT) == "BROWNOUT");
  CHECK(resetReasonText(ESP_RST_SW) == "SOFTWARE");
  CHECK(resetReasonText((esp_reset_reason_t)42) == "OTHER_42");
}

static void test_device() {
  WiFi.mac = "AA:BB:CC:DD:EE:FF";
  WiFi.ip.text = "10.0.0.5";
  ESP.freeHeap = 150000;
  ESP.minFreeHeap = 120000;
  fake_uptime_us = 3600LL * 1000000;
  IotTime ntp;  // not synced
  String d = iotStatusDevice(config(), ntp, ESP_RST_POWERON, 2);
  CHECK(has(d, "\"device\":{\"name\":\"backyard00\",\"firmware\":\"4.9.0\""));
  CHECK(has(d, "\"board\":\"ESP32 DEVKITV1 (ESP-WROOM-32)\""));
  CHECK(has(d, "\"cpu_speed\":80,"));
  CHECK(has(d, "\"mac\":\"AA:BB:CC:DD:EE:FF\",\"ip\":\"10.0.0.5\""));
  CHECK(has(d, "\"uptime\":3600,"));
  CHECK(has(d, "\"time\":\"not_synced\",\"time_synced\":false,"));
  CHECK(has(d, "\"reset_reason\":\"POWERON\""));
  CHECK(has(d, "\"free_heap\":150000,\"min_free_heap\":120000,\"wifi_reconnects\":2}"));

  String e = iotStatusDevice(config(), ntp, ESP_RST_SW, 0, "\"simulation\":true");
  CHECK(has(e, "\"wifi_reconnects\":0,\"simulation\":true}"));
}

static void test_network() {
  WiFi.rssi = -61;
  WiFi.bssid = "11:22:33:44:55:66";
  WiFi.ch = 6;
  String n = iotStatusNetwork(config());
  CHECK(n == "\"network\":{\"ssid\":\"home\",\"rssi\":-61,\"bssid\":\"11:22:33:44:55:66\","
             "\"channel\":6,\"hostname\":\"backyard00\"}");
}

static IotMqttConfig mqttConfig() {
  IotMqttConfig c;
  c.server = "broker.local";
  c.clientId = "backyard00";
  c.user = "backyard00";
  c.password = "secret";
  c.caCert = "-----BEGIN CERTIFICATE-----";
  return c;
}

static void test_mqtt() {
  fake_millis = 1000;
  WiFi.s = WL_CONNECTED;
  IotMqtt mqtt;
  mqtt.client().nextConnectOk = true;
  mqtt.begin(mqttConfig(), [](char*, uint8_t*, unsigned int) {}, nullptr);
  String m = iotStatusMqtt(config(), mqtt);
  CHECK(has(m, "\"mqtt\":{\"broker\":\"broker.local\",\"port\":8883,\"user\":\"backyard00\""));
  CHECK(has(m, "\"connected\":true,\"connects\":1,\"failed_attempts\":0,\"last_error\":\"\""));
  CHECK(!has(m, "disconnected_s"));  // only while disconnected
  CHECK(has(m, "\"last_error_uptime\":0,"));
  CHECK(has(m, "\"status_topic\":\"sensor/backyard/status\",\"telemetry_topic\":\"telemetry/backyard/device\"}"));

  // Connection lost: disconnected_s appears
  mqtt.client().up = false;
  mqtt.client().nextConnectOk = false;
  fake_millis = 61000;
  mqtt.loop();
  m = iotStatusMqtt(config(), mqtt);
  CHECK(has(m, "\"connected\":false"));
  CHECK(has(m, "\"disconnected_s\":"));
  CHECK(has(m, "\"failed_attempts\":1"));
}

int main() {
  test_helpers();
  test_device();
  test_network();
  test_mqtt();
  std::printf("iot_status: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
