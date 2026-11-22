# Changelog

## 0.6.0

- `iot_status`: the common sections of every device's `/status` JSON, the same fields on every device (iot-platform open item O12): `iotStatusDevice()` (name, firmware, board, cpu_speed, mac, ip, uptime, time, time_synced, reset_reason, free_heap, min_free_heap, wifi_reconnects, plus device extras), `iotStatusNetwork()` (ssid, rssi, bssid, channel, hostname) and `iotStatusMqtt()` (broker, port, user, connected, connects, failed_attempts, disconnected_s while disconnected, last_error, last_error_uptime, status_topic, telemetry_topic). Settings in `IotStatusConfig`; the board description is passed by the device.
- Text helpers moved from the devices, where they were identical: `jsonEscape()`, `htmlEscape()`, `u64ToString()`, `formatUptime()`, `resetReasonText()`.
- Example `status_basic`; host tests for `iot_status` (the `String`, `WiFi` and `ESP` stand-ins in `tests/host/fakes/` grew for it).

## 0.5.0

- `iot_ota` is opt-in: `esp32_iot_common.h` no longer includes it, and it is header-only (`iot_ota.cpp` removed). In 0.4.0 every sketch using the library linked the core's ArduinoOTA and mDNS code, 38–45 KB of flash even without OTA (the CI examples grew from 0.3.0 to 0.4.0: `mqtt_basic` +37.9 KB, `time_basic` +38.8 KB, `wifi_diag_basic` +44.7 KB). This breaks the rule that an unused feature costs nothing (CLAUDE.md rule 3).
- **Change for sketches:** a sketch that uses `IotOta` adds `#include <iot_ota.h>` after `#include <esp32_iot_common.h>`. Without it, 0.5.0 does not compile for such a sketch, hence the minor version step. No other change; `IotOta` behaves as in 0.4.0.

## 0.4.0

- `iot_ota`: `IotOta`, over-the-air updates from the Arduino tools (`ArduinoOTA` with host name and password) and mDNS (`<host>.local` with the `http` and `ota` services). An optional function runs when an update starts (the garage releases its outputs there in M3). Replaces each device's `setupOTA()` and mDNS code (iot-platform architecture §6, phase 6 step 4).
- Two small changes from the device code: the progress line no longer divides by zero for an image under 100 bytes (`progress * 100 / total`), and errors print as one line, `OTA Error[n]: text`.
- Example `ota_basic`; host tests for `IotOta` with stand-ins for `ArduinoOTA` and `ESPmDNS`.

## 0.3.0

- `iot_time`: `IotTime`, NTP time with a POSIX time zone (daylight saving automatic): `begin()` waits a limited time for the first sync (SNTP keeps trying in the background), `loop()` picks up a late sync, `synced()`, `epoch()`, `timestamp()`. Also `uptimeMillis()` and `uptimeSeconds()` from the 64-bit timer. Replaces each device's `setupTime()`, `getTimestamp()`, `getEpochTime()` and uptime helpers (iot-platform architecture §6, phase 6 step 4).
- One change from the door and shed code: `timestamp()` no longer waits up to 5 s when the time is not synced (the garage already did this).
- Example `time_basic`; host tests for `IotTime` with fake SNTP functions.

## 0.2.0

- `iot_mqtt`: `IotMqtt`, MQTT over TLS with a last will and a reconnect schedule (15 s for 5 minutes after boot or a lost connection, then 60 s), a 10 s TLS handshake limit, and counters and the last error for `/status`. TLS only with the CA certificate checked: no plain-text or insecure mode. Replaces each device's own connect/retry code (iot-platform architecture §6, phase 6 step 4).
- `retry_schedule.h`: the reconnect timing and MQTT state texts, host-tested (with stand-ins for Arduino, WiFi, TLS and PubSubClient in `tests/host/fakes/`).
- Example `mqtt_basic`; CI compiles all examples and installs PubSubClient (now a dependency).

## 0.1.1 (not tagged)

- Documentation and CI only: rule against board-specific code (README "Boards", CLAUDE.md); CI compiles the example with the Minimal SPIFFS partition scheme, as every device now does.

## 0.1.0

- First version: `wifi_diag` (WiFi and antenna diagnostics, roaming) and `stats_util` (percentile, roaming decision), moved unchanged from the device repositories, where the copies were identical (esp32-door at `621518b`). Umbrella header `esp32_iot_common.h`, example `wifi_diag_basic`, host tests, CI.
