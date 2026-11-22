// status_basic - a /status JSON page with the common device, network and mqtt sections.
//
// CI compiles this sketch to check iot_status against the real ESP32 core. Set
// your network, broker and CA certificate before uploading, then open
// http://<device IP>/status.

#include <WebServer.h>
#include <WiFi.h>
#include <esp32_iot_common.h>

const char* WIFI_SSID = "your-network";
const char* WIFI_PASSWORD = "your-password";
const char* CA_CERT = "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n";

IotMqtt mqtt;
IotTime ntp;
IotStatusConfig statusConfig;
WebServer server(80);
esp_reset_reason_t resetReason;

void onMessage(char*, uint8_t*, unsigned int) {}

void setup() {
  Serial.begin(115200);
  resetReason = esp_reset_reason();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  ntp.begin("PST8PDT,M3.2.0,M11.1.0", "pool.ntp.org", 30000);

  IotMqttConfig cfg;
  cfg.server = "broker.local";
  cfg.clientId = "example00";
  cfg.user = "example00";
  cfg.password = "change-me";
  cfg.caCert = CA_CERT;
  mqtt.begin(cfg, onMessage, nullptr);

  statusConfig.name = "example00";
  statusConfig.firmware = "1.0.0";
  statusConfig.board = "ESP32 Dev Module";
  statusConfig.ssid = WIFI_SSID;
  statusConfig.mqttServer = cfg.server;
  statusConfig.mqttPort = cfg.port;
  statusConfig.mqttUser = cfg.user;
  statusConfig.statusTopic = "sensor/example/status";
  statusConfig.telemetryTopic = "telemetry/example/device";

  server.on("/status", HTTP_GET, []() {
    String json = "{" + iotStatusDevice(statusConfig, ntp, resetReason, 0);
    json += ",\"sensors\":[]";  // the device's own sensors go here
    json += "," + iotStatusNetwork(statusConfig);
    json += "," + iotStatusMqtt(statusConfig, mqtt) + "}";
    server.send(200, "application/json", json);
  });
  server.begin();
}

void loop() {
  ntp.loop();
  mqtt.loop();
  server.handleClient();
  delay(10);
}
