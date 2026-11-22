// web_basic - the shared web pages: Home (common cards + a device card) and WiFi.
//
// CI compiles this sketch to check iot_web against the real ESP32 core and
// WebServer. Set your network, broker and CA certificate before uploading,
// then open http://<device IP>/. web_device.h holds web/device.js, compressed
// by tools/web_embed.py (run it again after editing device.js).

#include <WebServer.h>
#include <WiFi.h>
#if __has_include("version.h")
#include "version.h"
#endif
#include <esp32_iot_common.h>
#include <iot_web.h>  // opt-in: the web pages
#include "web_device.h"

const char* WIFI_SSID = "your-network";
const char* WIFI_PASSWORD = "your-password";
const char* CA_CERT = "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n";
const char* NAME = "example00";

IotMqtt mqtt;
IotTime ntp;
IotStatusConfig statusConfig;
WebServer server(80);
IotWeb web;
esp_reset_reason_t resetReason;
bool lamp = false;

void onMessage(char*, uint8_t*, unsigned int) {}

String statusJson() {
  String j = "{" + iotStatusDevice(statusConfig, ntp, resetReason, 0);
  j += ",\"sensors\":[{\"name\":\"Lamp\",\"type\":\"lamp\",\"state\":\"" + String(lamp ? "ON" : "OFF") + "\"}]";
  j += "," + iotStatusNetwork(statusConfig);
  j += "," + iotStatusMqtt(statusConfig, mqtt) + "}";
  return j;
}

void setup() {
  Serial.begin(115200);
  resetReason = esp_reset_reason();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  wifiDiag.begin(true);
  ntp.begin("PST8PDT,M3.2.0,M11.1.0", "pool.ntp.org", 30000);

  IotMqttConfig mc;
  mc.server = "broker.local";
  mc.clientId = NAME;
  mc.user = NAME;
  mc.password = "change-me";
  mc.caCert = CA_CERT;
  mqtt.begin(mc, onMessage, nullptr);

  statusConfig.name = NAME;
  statusConfig.firmware = FW_VERSION;
  statusConfig.build = FW_BUILD;
  statusConfig.board = "ESP32 Dev Module";
  statusConfig.ssid = WIFI_SSID;
  statusConfig.mqttServer = mc.server;
  statusConfig.mqttPort = mc.port;
  statusConfig.mqttUser = mc.user;
  statusConfig.statusTopic = "sensor/example/status";
  statusConfig.telemetryTopic = "telemetry/example/device";

  IotWebConfig wc;
  wc.user = "admin";
  wc.password = "change-me";
  wc.realm = NAME;
  wc.status = statusJson;
  wc.deviceJs = DEVICE_DEVICE_JS;
  wc.deviceJsLen = DEVICE_DEVICE_JS_LEN;
  web.begin(server, wc);

  // The lamp card's toggle: POST /api/lamp?on=1 or 0 (login required)
  server.on("/api/lamp", HTTP_POST, []() {
    if (!web.requireAuth()) return;
    lamp = webParseBool(server.arg("on").c_str());
    web.sendResult(200, true, "DONE", lamp ? "Lamp on" : "Lamp off");
  });
}

void loop() {
  server.handleClient();
  wifiDiag.loop();
  ntp.loop();
  mqtt.loop();
  delay(2);
}
