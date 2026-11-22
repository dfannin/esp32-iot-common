// mqtt_basic - MQTT over TLS with a last will and the reconnect schedule.
//
// CI compiles this sketch to check iot_mqtt against the real ESP32 core and
// PubSubClient. Set your network, broker and CA certificate before uploading.

#include <WiFi.h>
#include <esp32_iot_common.h>

const char* WIFI_SSID = "your-network";
const char* WIFI_PASSWORD = "your-password";
const char* CA_CERT = "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n";

IotMqtt mqtt;
PubSubClient& client = mqtt.client();

void onMessage(char* topic, uint8_t* payload, unsigned int length) {
  Serial.printf("[%s] %.*s\n", topic, (int)length, (const char*)payload);
}

void onConnected() {
  client.publish("sensor/example/status", "{\"state\":\"ONLINE\"}", true);
  client.subscribe("cmd/example/#");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  IotMqttConfig cfg;
  cfg.server = "broker.local";
  cfg.clientId = "example00";
  cfg.user = "example00";
  cfg.password = "password";
  cfg.caCert = CA_CERT;
  cfg.willTopic = "sensor/example/status";
  cfg.willPayload = "{\"state\":\"OFFLINE\"}";
  mqtt.begin(cfg, onMessage, onConnected);
}

void loop() {
  mqtt.loop();
  static unsigned long last = 0;
  if (mqtt.connected() && millis() - last >= 60000) {
    last = millis();
    client.publish("telemetry/example/device", "{\"type\":\"device\"}");
  }
}
