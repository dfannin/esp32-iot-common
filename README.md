# esp32-iot-common

Shared Arduino library for the home IoT ESP32 devices: [esp32-door](https://github.com/dfannin/esp32-door), [esp32-shed](https://github.com/dfannin/esp32-shed) and [esp32-garage](https://github.com/dfannin/esp32-garage). It replaces files that were copied into every device repository. The plan for the library is in [iot-platform `docs/architecture.md` §6](https://github.com/dfannin/iot-platform/blob/main/docs/architecture.md#6-shared-firmware-library).

## Contents

| Module | What it does |
|---|---|
| `iot_mqtt.h` / `.cpp` | MQTT over TLS with a last will and a reconnect schedule (`IotMqtt`): fast retries for 5 minutes after boot or a lost connection, then every minute; 10 s TLS handshake limit; connect count and last error for `/status`. TLS only, the broker certificate is always checked |
| `iot_status.h` / `.cpp` | The common sections of `/status` (`device`, `network`, `mqtt`), the same fields on every device, and the text helpers `jsonEscape()`, `htmlEscape()`, `u64ToString()`, `formatUptime()`, `resetReasonText()` |
| `iot_web.h` | The web pages (opt-in, header-only): the Home page with the common cards (Device Identity, System Status, Network Settings, MQTT Status) and the device's sensor cards, the WiFi page and its API, device pages, tabs and controls from the device's `device.js`; pages in `web/`, embedded compressed by `tools/web_embed.py`. See [Web Pages](#web-pages) |
| `web_args.h` | Checks numbers and switches sent by the web forms, no Arduino dependencies, host-tested |
| `iot_version.h` | Firmware version: `FW_VERSION` (`YYMMDD-HHMM`, `-dirty`, `-local`) and `FW_BUILD` (commit hash) from the sketch's `version.h`, with the `-local` fallback; `iotVersionValid()` |
| `tools/fw_version.sh` | Writes a sketch's `version.h` from git before each compile (committer time in UTC, `-dirty`, short hash) |
| `iot_time.h` / `.cpp` | NTP time with a POSIX time zone (`IotTime`): limited wait at boot, late sync picked up in `loop()`, epoch and local timestamp; `uptimeMillis()` / `uptimeSeconds()` from the 64-bit timer |
| `iot_ota.h` | Over-the-air updates from the Arduino tools and mDNS (`IotOta`): host name, password, `http` and `ota` services, optional function before an update starts. **Opt-in:** include `<iot_ota.h>` yourself; `esp32_iot_common.h` does not, so a sketch without OTA does not carry the core's ArduinoOTA and mDNS code (about 40 KB) |
| `retry_schedule.h` | The reconnect timing and MQTT state texts, no Arduino dependencies, host-tested |
| `wifi_diag.h` / `.cpp` | WiFi and antenna diagnostics: RSSI sampling and history, gateway ping (only while the WiFi page is open or during a reception test, unless `begin(true)`), reception tests, access-point scan, roaming to a stronger access point (global object `wifiDiag`) |
| `stats_util.h` | `percentile()` and `shouldRoam()`, no Arduino dependencies, host-tested |

Version 0.1.0 held the modules that were identical in all three device repositories; 0.2.0 adds the MQTT connection, 0.3.0 NTP time and uptime, 0.4.0 OTA updates and mDNS (opt-in since 0.5.0), 0.6.0 the common `/status` sections and text helpers, 0.7.0 firmware versions, 0.8.0 the web pages. The device web pages stay in each device (they are built differently); the self-description (Home Assistant discovery) and settings follow in later versions, one device at a time (iot-platform architecture §6, §15.2 phases 5–6).

Requires the **PubSubClient** library (Library Manager).

## Use in a Sketch

```cpp
#include <esp32_iot_common.h>   // finds the library and includes its modules

void setup() { wifiDiag.begin(); }
void loop()  { wifiDiag.loop(); }
```

Modules that pull in a large part of the ESP32 core are **opt-in**: the umbrella header leaves them out and a sketch includes them itself (today `#include <iot_ota.h>`), so a sketch pays only for what it uses.

See `examples/wifi_diag_basic/`, `examples/mqtt_basic/`, `examples/time_basic/`, `examples/ota_basic/`, `examples/status_basic/` and `examples/web_basic/`.

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

## Firmware Versions

Every device uses the same version convention (iot-platform architecture §6.4): `YYMMDD-HHMM` from the git committer time of the commit in UTC, `-dirty` for uncommitted changes, `-local` for a build without git, plus `FW_BUILD`, the short commit hash. The library itself keeps semantic versions.

A device sketch includes its generated `version.h` before the library and uses `FW_VERSION` and `FW_BUILD` (see `iot_version.h`); the device lists `version.h` in its `.gitignore`. Before each compile:

```bash
~/Arduino/libraries/esp32-iot-common/tools/fw_version.sh .    # in the sketch folder
```

## Web Pages

`iot_web.h` gives every device the same pages (iot-platform architecture §6.5). The page is one shell served compressed from flash; the browser draws it from `/status`, so the device builds no HTML.

| Page | Contents |
|---|---|
| **Home** (`/`) | Device Identity, the device's sensor cards, System Status, Network Settings, MQTT Status |
| **WiFi** (`/wifi`) | Signal, roaming, reception tests, TX power and modem sleep, nearby networks |
| **JSON** (`/status`) | The raw `/status` |
| Device pages | Defined in the device's `device.js`, served by `web.addPage(path)` |

A device adds what is its own in `web/device.js` (embedded with `tools/web_embed.py` into its `src/web_device.h`):

```js
IOT.setup({title: 'ESP32 Garage', icon: '&#x1F697;', sensors: 'Garage Doors'});
IOT.card('garage_door', (d, st) => `<div class='sensor'><span class='label'>${esc(d.name)}:</span>${badge(d.state)}` +
  IOT.button('Open', `/api/door/${d.id}/open`, {confirm: 'Open the door?'}) +
  IOT.toggle('Light', d.light, `/api/door/${d.id}/light_on`, `/api/door/${d.id}/light_off`) + '</div>');
IOT.page({path: '/sim', tab: 'Simulator', show: st => st.device.simulation,
          html: () => "<div id='doors'></div>", load: async st => { /* fill #doors */ }});
```

| Extension | What it does |
|---|---|
| `IOT.setup({title, icon, sensors})` | Page title and icon, heading of the sensor cards |
| `IOT.card(type, fn)` | Card for the sensors of one `type` in `/status`; other types get the default card (name, state, every plain field, topic) |
| `IOT.page({path, tab, html, load, refresh, show})` | A page with its own tab: `html()` once, `load(st)` on every refresh (default 2 s), `show(st)` hides the tab |
| `IOT.banner(fn)` | A line shown on every page, e.g. "simulation" |
| `IOT.button(label, url, {confirm})` | Sends a POST, shows the device's answer, refreshes |
| `IOT.toggle(label, on, onUrl, offUrl)` | On/off button |
| `IOT.form(id, fields, url, {submit})` | Number, text, list and checkbox fields posted as parameters; `IOT.fill(id, values)` sets them from the device unless the user is editing |
| `IOT.action(name, fn)` | A button that runs a function instead of a POST |

Helpers: `esc()`, `badge()`, `kv()`, `yes()`, `dur()`. On the device, endpoints use `web.requireAuth()` (login for every change), `web.sendResult()` (the answer the pages show) and `web_args.h`. See `examples/web_basic/`.

**Editing the pages:** change the files in `web/`, then regenerate the embedded header (CI fails if it is out of date) and run the browser tests:

```bash
python3 tools/web_embed.py src/iot_web_assets.h IOT_WEB web/page.html web/style.css web/app.js
```

```bash
npm install --no-save --no-package-lock playwright@1.63.0 && node tests/web/run.js
```

## Develop

```bash
make -C tests/host                                                     # host unit tests
for ex in examples/*/; do arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs --library . "$ex"; done
```

**Versions:** bump `version` in `library.properties` and `ESP32_IOT_COMMON_VERSION` in `src/esp32_iot_common.h` together and add a line to `CHANGELOG.md`. After the merge to `main`, the `tag` workflow (`.github/workflows/tag.yml`) creates the tag `vX.Y.Z` on that commit (it stops with an error if the two version numbers differ, and does nothing if the tag already exists).

## License

BSD 2-Clause, as the device repositories.
