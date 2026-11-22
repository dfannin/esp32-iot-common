// time_basic - NTP time with the local time zone, and uptime since boot.
//
// CI compiles this sketch to check iot_time against the real ESP32 core.
// Set your network before uploading.

#include <WiFi.h>
#include <esp32_iot_common.h>

const char* WIFI_SSID = "your-network";
const char* WIFI_PASSWORD = "your-password";

IotTime ntp;
uint32_t lastPrint = 0;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  // POSIX time zone: daylight saving changes happen automatically
  ntp.begin("PST8PDT,M3.2.0,M11.1.0", "pool.ntp.org", 30000);
}

void loop() {
  ntp.loop();  // picks up the time if it was not available at boot
  if (millis() - lastPrint >= 10000) {
    lastPrint = millis();
    Serial.printf("%s  epoch %lu  uptime %llu s\n", ntp.timestamp().c_str(),
                  ntp.synced() ? ntp.epoch() : 0UL, (unsigned long long)uptimeSeconds());
  }
  delay(100);
}
