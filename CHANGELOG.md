# Changelog

## Unreleased

- Documentation and CI only: rule against board-specific code (README "Boards", CLAUDE.md); CI compiles the example with the Minimal SPIFFS partition scheme, as every device now does.

## 0.1.0

- First version: `wifi_diag` (WiFi and antenna diagnostics, roaming) and `stats_util` (percentile, roaming decision), moved unchanged from the device repositories, where the copies were identical (esp32-door at `621518b`). Umbrella header `esp32_iot_common.h`, example `wifi_diag_basic`, host tests, CI.
