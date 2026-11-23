# Changelog

## 0.9.0

- `wifi_diag`: the gateway is pinged only while the WiFi page is open (for a minute after its last request) and during a reception test. `begin()` now defaults to `backgroundPing = false` (was `true`); `begin(true)` still pings all the time, for troubleshooting. Pinging once a second kept the radio busy and its statistics were sent in the 5-minute telemetry, which is no longer wanted (iot-platform architecture 0.26). Reception tests keep their ping results.
- Behavior change for sketches calling `begin()` without an argument: they stop pinging in the background.

## 0.8.1

- WiFi page: the "Signal, Last 10 Minutes" graph keeps -95 to -25 dBm by default and widens in 10 dB steps when the signal is stronger or weaker. A signal above -25 dBm (esp32-garage at -21 dBm, next to its access point) was drawn above the graph and looked as if the graph had stopped. The limit came from the garage's old page.
- Browser tests: the graph's range and that the line is drawn at -60, -21 and -97 dBm.

## 0.8.0

- `iot_web` (opt-in, header-only: `#include <iot_web.h>`): the web pages for every device (iot-platform architecture §6.5). One compressed page shell from flash; the browser draws it from `/status` with `/app.js`:
  - **Home**: the common cards Device Identity (name, chip model, board, firmware with build), System Status (CPU speed, time, time sync, uptime, last reset, reconnects, free memory), Network Settings (SSID, signal quality, IP, MAC, mDNS host name, access point and channel), MQTT Status (status, failed attempts and last error when there are any, broker, port, username, status and telemetry topics), and the device's sensor cards (a default card for any sensor type).
  - **WiFi**: the WiFi diagnostics page and its `/api/wifi...` endpoints, moved from the devices.
  - **Device extensions** in the device's `/device.js`: `IOT.setup()` (title, icon, sensors heading), `IOT.card()` (sensor card per type), `IOT.page()` (own pages and tabs, shown conditionally), `IOT.banner()`, and controls `IOT.button()` (POST with optional confirmation), `IOT.toggle()`, `IOT.form()` (fields posted as parameters, filled from the device without overwriting edits), `IOT.action()`.
  - C++: `IotWeb::begin()`, `addPage()`, `requireAuth()`, `sendJson()`, `sendResult()`; `web_args.h` checks numbers and switches sent by forms (`webParseLong`, `webParseFloat`, `webParseBool`).
- Pages are edited as real files in `web/` and embedded compressed (gzip, about 60 % smaller) by `tools/web_embed.py`, which devices also use for their `device.js`; `--check` keeps the generated headers honest in CI.
- `iot_status`: new `device.chip`, read from the chip (`ESP.getChipModel()` and revision), e.g. `ESP32-D0WD-V3 rev 3`.
- Tests: `test_iot_web` (host, with `WebServer` and `wifi_diag` stand-ins) and `tests/web/run.js` (the pages in headless Chromium with fixture data: cards, tabs, controls, forms, the WiFi page), both in CI. Example `web_basic`.
- Change from the devices' WiFi endpoints: `seconds` and `tx_power` are checked strictly (a value such as `60s` is rejected instead of read as 60).

## 0.7.0

- Firmware versions (iot-platform architecture §6.4, O13): `tools/fw_version.sh` writes `version.h` in a sketch folder from git, with `FW_VERSION` = `YYMMDD-HHMM` from the committer time of `HEAD` in UTC (`-dirty` with uncommitted changes to tracked files) and `FW_BUILD` = the short commit hash. It rewrites the file only when it changes.
- `iot_version.h`: the `-local` fallback (`FW_VERSION` from the compiler's build time, `FW_BUILD` empty) when a sketch has no `version.h`, computed at compile time; `iotVersionValid()` checks the convention.
- `iot_status`: `IotStatusConfig.build`, shown as `device.build` after `firmware` in `/status`.
- Tests: `test_iot_version` (host) and `tests/test_fw_version.sh` (the script in a temporary git repository), both in CI. Example `status_basic` reports `FW_VERSION` and `FW_BUILD`.
- CI: the `tag` workflow creates `vX.Y.Z` after a merge to `main` when `library.properties` has a new version (and fails if it differs from `ESP32_IOT_COMMON_VERSION`). Releases no longer need a manual tag.

- CI: the `tag` workflow creates `vX.Y.Z` after a merge to `main` when `library.properties` has a new version (and fails if it differs from `ESP32_IOT_COMMON_VERSION`). Releases no longer need a manual tag.

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
