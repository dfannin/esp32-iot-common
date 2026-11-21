# esp32-iot-common

Shared Arduino library for the home IoT ESP32 devices: [esp32-door](https://github.com/dfannin/esp32-door), [esp32-shed](https://github.com/dfannin/esp32-shed) and [esp32-garage](https://github.com/dfannin/esp32-garage). It replaces files that were copied into every device repository. The plan for the library is in [iot-platform `docs/architecture.md` §6](https://github.com/dfannin/iot-platform/blob/main/docs/architecture.md#6-shared-firmware-library).

## Contents

| Module | What it does |
|---|---|
| `wifi_diag.h` / `.cpp` | WiFi and antenna diagnostics: RSSI sampling and history, gateway ping, reception tests, access-point scan, roaming to a stronger access point (global object `wifiDiag`) |
| `stats_util.h` | `percentile()` and `shouldRoam()`, no Arduino dependencies, host-tested |

Version 0.1.0 holds the modules that were identical in all three device repositories. MQTT over TLS, NTP, OTA, the device web page, the self-description (Home Assistant discovery) and settings follow in later versions, one device at a time (iot-platform architecture §6, §15.2 phases 5–6).

## Use in a Sketch

```cpp
#include <esp32_iot_common.h>   // finds the library and includes its modules

void setup() { wifiDiag.begin(true); }
void loop()  { wifiDiag.loop(); }
```

See `examples/wifi_diag_basic/`.

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
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs --library . examples/wifi_diag_basic
```

**Versions:** bump `version` in `library.properties` and `ESP32_IOT_COMMON_VERSION` in `src/esp32_iot_common.h` together, add a line to `CHANGELOG.md`, merge, then tag `vX.Y.Z` on `main`.

## License

BSD 2-Clause, as the device repositories.
