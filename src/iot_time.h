// iot_time.h - NTP time with the local time zone, and uptime since boot.
//
// Replaces the setupTime()/getTimestamp()/getEpochTime() code that each device
// had. begin() waits a limited time for the first sync: after a power outage the
// router may be up before the internet. SNTP keeps trying in the background and
// loop() notices when the time arrives.
//
//   IotTime ntp;   // not "clock": that is a C library function
//
//   void setup() { ntp.begin(tz_info, ntp_server, NTP_TIMEOUT); }  // after WiFi
//   void loop()  { ntp.loop(); }
//
//   if (ntp.synced()) message += ",\"epoch\":" + String(ntp.epoch());

#pragma once

#include <Arduino.h>
#include <esp_timer.h>
#include <time.h>

// Uptime from the 64-bit microsecond timer. millis() is 32 bits and wraps after
// about 49.7 days; interval checks ("millis() - last >= interval") still work
// across the wrap, so only an uptime that is shown needs the bigger counter.
inline uint64_t uptimeMillis() { return (uint64_t)esp_timer_get_time() / 1000ULL; }
inline uint64_t uptimeSeconds() { return (uint64_t)esp_timer_get_time() / 1000000ULL; }

class IotTime {
 public:
  // Starts SNTP with a POSIX time zone (for example "PST8PDT,M3.2.0,M11.1.0",
  // so daylight saving changes happen automatically) and waits up to waitMs for
  // the first sync. Returns synced().
  bool begin(const char* tz, const char* server, uint32_t waitMs = 30000);

  // Call every loop(): picks up the time if it was not available at boot.
  void loop();

  bool synced() const { return synced_; }

  // Seconds since 1970 (UTC); meaningful only when synced().
  unsigned long epoch() const;

  // Local time as "YYYY-MM-DDTHH:MM:SS", or notSynced before the first sync.
  String timestamp(const char* notSynced = "Time not synced") const;

 private:
  bool synced_ = false;
};
