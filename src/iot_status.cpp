// iot_status.cpp - see iot_status.h.

#include "iot_status.h"

#include <WiFi.h>

static String jsonText(const char* key, const String& value) {
  return "\"" + String(key) + "\":\"" + jsonEscape(value) + "\"";
}

String iotStatusDevice(const IotStatusConfig& cfg, const IotTime& ntp, esp_reset_reason_t resetReason,
                       uint32_t wifiReconnects, const String& extra) {
  String j = "\"device\":{";
  j += jsonText("name", cfg.name) + ",";
  j += jsonText("firmware", cfg.firmware) + ",";
  j += jsonText("build", cfg.build) + ",";
  j += jsonText("board", cfg.board) + ",";
  j += "\"cpu_speed\":" + String(getCpuFrequencyMhz()) + ",";
  j += jsonText("mac", WiFi.macAddress()) + ",";
  j += jsonText("ip", WiFi.localIP().toString()) + ",";
  j += "\"uptime\":" + u64ToString(uptimeSeconds()) + ",";
  j += jsonText("time", ntp.timestamp("not_synced")) + ",";
  j += "\"time_synced\":" + String(ntp.synced() ? "true" : "false") + ",";
  j += jsonText("reset_reason", resetReasonText(resetReason)) + ",";
  j += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  j += "\"min_free_heap\":" + String(ESP.getMinFreeHeap()) + ",";
  j += "\"wifi_reconnects\":" + String(wifiReconnects);
  if (extra.length() > 0) j += "," + extra;
  j += "}";
  return j;
}

String iotStatusNetwork(const IotStatusConfig& cfg) {
  String j = "\"network\":{";
  j += jsonText("ssid", cfg.ssid) + ",";
  j += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  j += jsonText("bssid", WiFi.BSSIDstr()) + ",";
  j += "\"channel\":" + String(WiFi.channel()) + ",";
  j += jsonText("hostname", cfg.name);
  j += "}";
  return j;
}

String iotStatusMqtt(const IotStatusConfig& cfg, IotMqtt& mqtt) {
  bool connected = mqtt.connected();
  String j = "\"mqtt\":{";
  j += jsonText("broker", cfg.mqttServer) + ",";
  j += "\"port\":" + String((unsigned int)cfg.mqttPort) + ",";
  j += jsonText("user", cfg.mqttUser) + ",";
  j += "\"connected\":" + String(connected ? "true" : "false") + ",";
  j += "\"connects\":" + String(mqtt.connects()) + ",";
  j += "\"failed_attempts\":" + String(mqtt.failedAttempts()) + ",";
  if (!connected) j += "\"disconnected_s\":" + String(mqtt.downForS()) + ",";
  j += jsonText("last_error", mqtt.lastError()) + ",";
  j += "\"last_error_uptime\":" + u64ToString(mqtt.lastErrorUptimeS()) + ",";
  j += jsonText("status_topic", cfg.statusTopic) + ",";
  j += jsonText("telemetry_topic", cfg.telemetryTopic);
  j += "}";
  return j;
}

// The helpers below were identical in esp32-door, esp32-shed and esp32-garage.

String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (unsigned int i = 0; i < in.length(); i++) {
    char c = in.charAt(i);
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04x", (unsigned int)(uint8_t)c);
          out += buf;
        } else {
          out += c;  // UTF-8 bytes (e.g. degree sign) pass through unchanged
        }
    }
  }
  return out;
}

String htmlEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (unsigned int i = 0; i < in.length(); i++) {
    char c = in.charAt(i);
    switch (c) {
      case '&':  out += "&amp;"; break;
      case '<':  out += "&lt;"; break;
      case '>':  out += "&gt;"; break;
      case '"':  out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default:   out += c;
    }
  }
  return out;
}

String u64ToString(uint64_t value) {
  char buf[21];
  snprintf(buf, sizeof(buf), "%llu", (unsigned long long)value);
  return String(buf);
}

String formatUptime(uint64_t totalSeconds) {
  unsigned long seconds = totalSeconds % 60;
  unsigned long minutes = (totalSeconds / 60) % 60;
  unsigned long hours = (totalSeconds / 3600) % 24;
  unsigned long days = totalSeconds / 86400;

  String uptime = "";
  if (days > 0) uptime += String(days) + "d ";
  if (hours > 0) uptime += String(hours) + "h ";
  if (minutes > 0) uptime += String(minutes) + "m ";
  uptime += String(seconds) + "s";
  return uptime;
}

String resetReasonText(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXTERNAL";
    case ESP_RST_SW:        return "SOFTWARE";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WATCHDOG";
    case ESP_RST_TASK_WDT:  return "TASK_WATCHDOG";
    case ESP_RST_WDT:       return "WATCHDOG";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    case ESP_RST_UNKNOWN:   return "UNKNOWN";
    default:                return "OTHER_" + String((int)reason);
  }
}
