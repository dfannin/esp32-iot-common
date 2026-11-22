# CLAUDE.md — esp32-iot-common

Arduino library shared by the ESP32 device firmware (`esp32-door`, `esp32-shed`, `esp32-garage`). Part of the home IoT platform; the plan is in `dfannin/iot-platform` `docs/architecture.md` §6 (shared firmware library) and §15.2 (phases 5–6).

## Rules that must not be broken

1. **No board-specific code.** The devices use different boards (ESP32 DEVKITV1 with ESP-WROOM-32 for esp32-door and esp32-shed; ESP32-DevKitC-V4 with ESP32-WROOM-32UE, external antenna and no user LED, for esp32-garage). The library has no pin numbers, LED handling or board checks; anything board-specific stays in the device's `settings.h` and is passed to the library (a function argument or a setting).
2. **No secrets or personal identifiers.** This repository is public. No WiFi or MQTT credentials, certificates, IP or MAC addresses; examples use placeholders.
3. **Memory matters** (architecture §6.1): fixed-size buffers, no growth on the heap in the loop, `PROGMEM` for large constant text. New optional features go behind compile-time switches (`IOT_FEATURE_*`) or are opt-in headers that `esp32_iot_common.h` does not include (like `iot_ota.h`, header-only so no library `.cpp` pulls in ArduinoOTA), so a device that does not use them pays nothing. Check the example sizes in CI when adding a module.
4. **Low power** (architecture §6.2): no busy loops; the radio in short bursts.
5. **Devices connect with TLS only** (architecture decision A9): no plain-text MQTT fallback in library code.
6. **One version, one tag.** `library.properties` `version` and `ESP32_IOT_COMMON_VERSION` change together; devices pin a tag in CI.

## Environment

- ESP32 Arduino core 3.3.13 (pinned in CI, like the device repositories).
- Every device builds with the ESP32 Dev Module and the **Minimal SPIFFS** partition scheme (1.9 MB app slots with OTA; iot-platform architecture §6.1), so CI compiles the example the same way.
- Host tests: `g++` with C++17; `tests/host/fakes/` has small stand-ins for Arduino, WiFi, WiFiClientSecure and PubSubClient so Arduino-facing modules (like `iot_mqtt`) can be tested on the host.
- Dependency: PubSubClient (Library Manager).

## Commands

```bash
make -C tests/host
for ex in examples/*/; do arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs --library . "$ex"; done
```

## Working style

- Move code from a device into the library unchanged first; change behaviour in a separate commit.
- Keep logic that can run without hardware in plain C++ headers (like `stats_util.h`) and test it on the host.
- Small, readable code; documents in GitHub Markdown.
