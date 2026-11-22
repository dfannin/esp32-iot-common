// ota_basic - over-the-air updates from the Arduino tools, and mDNS.
//
// CI compiles this sketch to check iot_ota against the real ESP32 core. Set
// your network and an OTA password before uploading; later uploads go over WiFi:
//   arduino-cli compile --upload -p <device IP> -l network .

#include <WiFi.h>
#include <esp32_iot_common.h>
#include <iot_ota.h>  // opt-in module, not included by esp32_iot_common.h

const char* WIFI_SSID = "your-network";
const char* WIFI_PASSWORD = "your-password";
const char* HOSTNAME = "example00";
const char* OTA_PASSWORD = "change-me";

IotOta ota;

void beforeUpdate() { Serial.println("Stopping outputs before the update"); }

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  ota.begin(HOSTNAME, OTA_PASSWORD, beforeUpdate);  // example00.local
}

void loop() {
  ota.loop();
  delay(10);
}
