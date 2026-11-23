// wifi_diag_basic - the smallest use of esp32-iot-common.
//
// Connects to WiFi, runs the WiFi diagnostics and prints the rolling statistics
// every 30 seconds. CI compiles this sketch to check the library on its own.
// Set your network below before uploading.

#include <WiFi.h>
#include <esp32_iot_common.h>

const char* WIFI_SSID = "your-network";
const char* WIFI_PASSWORD = "your-password";

unsigned long lastPrint = 0;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiDiag.begin(true);  // troubleshooting: ping the gateway all the time (the default pings only for the WiFi page)
  wifiDiag.setRoaming(true, 2UL * 60 * 60 * 1000, 8);
  Serial.printf("esp32-iot-common %s\n", ESP32_IOT_COMMON_VERSION);
}

void loop() {
  wifiDiag.loop();
  if (millis() - lastPrint >= 30000) {
    lastPrint = millis();
    WifiStats s = wifiDiag.takeWindow();
    Serial.printf("RSSI avg %.1f dBm (min %d, max %d), ping loss %.0f %%, roams %lu\n",
                  s.rssiAvg, s.rssiMin, s.rssiMax, s.pingLoss * 100,
                  (unsigned long)wifiDiag.roams());
  }
}
