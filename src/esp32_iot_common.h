// esp32_iot_common.h - shared code for the home IoT ESP32 devices.
//
// Include this header from a sketch: it makes the Arduino tools find the library
// and brings in its modules.
//
//   #include <esp32_iot_common.h>
//
// Modules:
//   iot_mqtt.h        MQTT over TLS with a last will and a reconnect schedule (IotMqtt)
//   iot_time.h        NTP time with the local time zone (IotTime), uptime since boot
//   iot_ota.h         over-the-air updates and mDNS (IotOta); opt-in, include it yourself:
//                     #include <iot_ota.h>  (keeps ArduinoOTA out of sketches without OTA)
//   wifi_diag.h       WiFi and antenna diagnostics, roaming (global object wifiDiag)
//   stats_util.h      percentile() and shouldRoam(), no Arduino dependencies
//   retry_schedule.h  reconnect timing and MQTT state texts, no Arduino dependencies

#pragma once

#define ESP32_IOT_COMMON_VERSION "0.5.0"

#include "iot_mqtt.h"
#include "iot_time.h"
#include "retry_schedule.h"
#include "stats_util.h"
#include "wifi_diag.h"
