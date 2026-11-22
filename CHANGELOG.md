# Changelog

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
