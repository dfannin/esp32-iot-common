// retry_schedule.h - MQTT reconnect timing and state texts (no Arduino dependencies; host-tested)
//
// After boot, or after the connection is lost, retry every fastMs for fastPeriodMs,
// then every slowMs. A short outage (broker restart, WiFi drop, a device restart
// after an OTA update) is over within seconds; a long broker outage does not keep
// blocking the loop with TLS attempts.

#pragma once

#include <stdint.h>

struct RetrySchedule {
  uint32_t fastMs;        // interval while the outage is new
  uint32_t fastPeriodMs;  // how long the fast interval applies
  uint32_t slowMs;        // interval after that

  // Interval to use now, for an outage that started at downSince (millis()).
  // Unsigned arithmetic, so it stays correct when millis() wraps (49.7 days).
  uint32_t intervalMs(uint32_t now, uint32_t downSince) const {
    return (now - downSince < fastPeriodMs) ? fastMs : slowMs;
  }

  // Is an attempt due? attempted is false until the first attempt of an outage,
  // which is then due at once.
  bool due(uint32_t now, uint32_t downSince, uint32_t lastAttempt, bool attempted) const {
    return !attempted || now - lastAttempt >= intervalMs(now, downSince);
  }
};

// PubSubClient state() as text, for logs and /status.
inline const char* mqttStateText(int state) {
  switch (state) {
    case -4: return "timeout";
    case -3: return "connection lost";
    case -2: return "connect failed";
    case -1: return "disconnected";
    case 0: return "connected";
    case 1: return "bad protocol";
    case 2: return "bad client id";
    case 3: return "server unavailable";
    case 4: return "bad user or password";
    case 5: return "not authorized";
    default: return "unknown state";
  }
}
