// esp32_iot_common.h - shared code for the home IoT ESP32 devices.
//
// Include this header from a sketch: it makes the Arduino tools find the library
// and brings in its modules.
//
//   #include <esp32_iot_common.h>
//
// Modules:
//   wifi_diag.h   WiFi and antenna diagnostics, roaming (global object wifiDiag)
//   stats_util.h  percentile() and shouldRoam(), no Arduino dependencies

#pragma once

#define ESP32_IOT_COMMON_VERSION "0.1.0"

#include "stats_util.h"
#include "wifi_diag.h"
