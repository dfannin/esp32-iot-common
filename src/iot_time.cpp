// iot_time.cpp - see iot_time.h.

#include "iot_time.h"

bool IotTime::begin(const char* tz, const char* server, uint32_t waitMs) {
  Serial.print("Setting up NTP time sync...");
  configTzTime(tz, server);

  struct tm timeinfo;
  uint32_t start = millis();
  while (!getLocalTime(&timeinfo, 1000)) {
    Serial.print(".");
    if (millis() - start >= waitMs) {
      Serial.println("\nNTP not available, continuing without time");
      return false;
    }
  }
  synced_ = true;
  Serial.printf("\nTime synced: %s\n", timestamp().c_str());
  return true;
}

void IotTime::loop() {
  if (synced_) return;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 0)) {  // 0: do not wait
    synced_ = true;
    Serial.println("Time synced");
  }
}

unsigned long IotTime::epoch() const {
  time_t now;
  time(&now);
  return (unsigned long)now;
}

String IotTime::timestamp(const char* notSynced) const {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) return String(notSynced);
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  return String(buf);
}
