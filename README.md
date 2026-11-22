# esp32-iot-common

Shared Arduino library for the home IoT ESP32 devices: [esp32-door](https://github.com/dfannin/esp32-door), [esp32-shed](https://github.com/dfannin/esp32-shed) and [esp32-garage](https://github.com/dfannin/esp32-garage). It replaces files that were copied into every device repository. The plan for the library is in [iot-platform `docs/architecture.md` §6](https://github.com/dfannin/iot-platform/blob/main/docs/architecture.md#6-shared-firmware-library).

## Contents

| Module | What it does |
|---|---|
| `iot_mqtt.h` / `.cpp` | MQTT over TLS with a last will and a reconnect schedule (`IotMqtt`): fast retries for 5 minutes after boot or a lost connection, then every minute; 10 s TLS handshake limit; connect count and last error for `/status`. TLS only, the broker certificate is always checked |
| `iot_status.h` / `.cpp` | The common sections of `/status` (`device`, `network`, `mqtt`), the same fields on every device, and the text helpers `jsonEscape()`, `htmlEscape()`, `u64ToString()`, `formatUptime()`, `resetReasonText()` |
| `iot_time.h` / `.cpp` | NTP time with a POSIX time zone (`IotTime`): limited wait at boot, late sync picked up in `loop()`, epoch and local timestamp; `uptimeMillis()` / `uptimeSeconds()` from the 64-bit timer |
| `iot_ota.h` | Over-the-air updates from the Arduino tools and mDNS (`IotOta`): host name, password, `http` and `ota` services, optional function before an update starts. **Opt-in:** include `<iot_ota.h>` yourself; `esp32_iot_common.h` does not, so a sketch without OTA does not carry the core's ArduinoOTA and mDNS code (about 40 KB) |
| `retry_schedule.h` | The reconnect timing and MQTT state texts, no Arduino dependencies, host-tested |
| `wifi_diag.h` / `.cpp` | WiFi and antenna diagnostics: RSSI sampling and history, gateway ping, reception tests, access-point scan, roaming to a stronger access point (global object `wifiDiag`) |
| `stats_util.h` | `percentile()` and `shouldRoam()`, no Arduino dependencies, host-tested |

Version 0.1.0 held the modules that were identical in all three device repositories; 0.2.0 adds the MQTT connection, 0.3.0 NTP time and uptime, 0.4.0 OTA updates and mDNS (opt-in since 0.5.0), 0.6.0 the common `/status` sections and text helpers. The device web pages stay in each device (they are built differently); the self-description (Home Assistant discovery) and settings follow in later versions, one device at a time (iot-platform architecture §6, §15.2 phases 5–6).

Requires the **PubSubClient** library (Library Manager).

## Use in a Sketch

```cpp
#include <esp32_iot_common.h>   // finds the library and includes its modules

void setup() { wifiDiag.begin(true); }
void loop()  { wifiDiag.loop(); }
```

Modules that pull in a large part of the ESP32 core are **opt-in**: the umbrella header leaves them out and a sketch includes them itself (today `#include <iot_ota.h>`), so a sketch pays only for what it uses.

See `examples/wifi_diag_basic/`, `examples/mqtt_basic/`, `examples/time_basic/`, `examples/ota_basic/` and `examples/status_basic/`.

## Install

The library is used from a checkout, linked into the Arduino libraries folder, so the Arduino IDE and `arduino-cli` both find it:

```bash
git clone https://github.com/dfannin/esp32-iot-common.git ~/src/iot/esp32-iot-common
ln -s ~/src/iot/esp32-iot-common ~/Arduino/libraries/esp32-iot-common
```

Each device's CI checks out a **pinned tag** (for example `v0.1.0`), so a device builds against a known version; a device moves to a newer version in its own change.

Requires the ESP32 Arduino core 3.x (built and tested with 3.3.13).

## Boards

The devices use different boards: ESP32 DEVKITV1 (ESP-WROOM-32) for esp32-door and esp32-shed, ESP32-DevKitC-V4 (ESP32-WROOM-32UE, external antenna, no user LED) for esp32-garage. All build as **ESP32 Dev Module** with the **Minimal SPIFFS** partition scheme (1.9 MB app slots with OTA, 128 KB SPIFFS, which no device uses).

**The library has no board-specific code:** no pin numbers, LED handling or board checks. Anything that depends on the board stays in the device's `settings.h` and is passed to the library.

## Develop

```bash
make -C tests/host                                                     # host unit tests
for ex in examples/*/; do arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs --library . "$ex"; done
```

**Versions:** bump `version` in `library.properties` and `ESP32_IOT_COMMON_VERSION` in `src/esp32_iot_common.h` together and add a line to `CHANGELOG.md`. After the merge to `main`, the `tag` workflow (`.github/workflows/tag.yml`) creates the tag `vX.Y.Z` on that commit (it stops with an error if the two version numbers differ, and does nothing if the tag already exists).

## License

BSD 2-Clause, as the device repositories.
