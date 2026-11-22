// iot_status.h - the common parts of a device's /status JSON, and text helpers.
//
// Every device's /status has the same "device", "network" and "mqtt" sections
// (iot-platform architecture §6, open item O12); the device adds its own
// "sensors" and anything else. Each function returns one JSON member such as
// "\"device\":{...}" to place inside the device's object:
//
//   IotStatusConfig statusConfig;   // filled in setup()
//
//   String json = "{" + iotStatusDevice(statusConfig, ntp, resetReason, wifiReconnects);
//   json += ",\"sensors\":[...]";
//   json += "," + iotStatusNetwork(statusConfig);
//   json += "," + iotStatusMqtt(statusConfig, mqtt) + "}";
//
// Fields (the same on every device):
//   device:  name, firmware, build, board, cpu_speed, mac, ip, uptime, time, time_synced,
//            reset_reason, free_heap, min_free_heap, wifi_reconnects (+ device extras)
//   network: ssid, rssi, bssid, channel, hostname
//   mqtt:    broker, port, user, connected, connects, failed_attempts,
//            disconnected_s (only while disconnected), last_error, last_error_uptime,
//            status_topic, telemetry_topic

#pragma once

#include <Arduino.h>
#include <esp_system.h>

#include "iot_mqtt.h"
#include "iot_time.h"

struct IotStatusConfig {
  const char* name = "";       // the MQTT client id, also the mDNS host name
  const char* firmware = "";   // FW_VERSION: "YYMMDD-HHMM", "-dirty" or "-local" (architecture §6.4)
  const char* build = "";      // FW_BUILD: short commit hash, "" for -local builds
  const char* board = "";      // board description (board-specific, set by the device)
  const char* ssid = "";
  const char* mqttServer = "";
  uint16_t mqttPort = 8883;
  const char* mqttUser = "";
  const char* statusTopic = "";
  String telemetryTopic;       // telemetry/<location>/device
};

// "\"device\":{...}". extra, if not empty, is added inside the object
// (for example "\"simulation\":true").
String iotStatusDevice(const IotStatusConfig& cfg, const IotTime& ntp, esp_reset_reason_t resetReason,
                       uint32_t wifiReconnects, const String& extra = "");

// "\"network\":{...}"
String iotStatusNetwork(const IotStatusConfig& cfg);

// "\"mqtt\":{...}"
String iotStatusMqtt(const IotStatusConfig& cfg, IotMqtt& mqtt);

// Text helpers used by every device's JSON and web pages.
String jsonEscape(const String& in);     // for a JSON string value
String htmlEscape(const String& in);     // for HTML text and attribute values
String u64ToString(uint64_t value);      // String() has no 64-bit constructor
String formatUptime(uint64_t totalSeconds);        // "3d 4h 5m 6s"
String resetReasonText(esp_reset_reason_t reason);  // "POWERON", "BROWNOUT", ...
