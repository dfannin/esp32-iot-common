// iot_web.h - the device web pages (iot-platform architecture §6.5).
//
// Opt-in, header-only, like iot_ota.h: esp32_iot_common.h does not include it,
// so a sketch without a web server does not link WebServer and the pages.
//
// Every page is the same compressed shell from flash; the browser draws it
// from /status with /app.js. The library serves the Home and WiFi pages, the
// stylesheet, the script, /status and the /api/wifi... endpoints. The device
// adds its own sensor cards, pages and controls in its /device.js (web/device.js,
// embedded with tools/web_embed.py) and its own endpoints:
//
//   #include <iot_web.h>
//   #include "src/web_device.h"     // DEVICE_DEVICE_JS from web/device.js
//
//   WebServer server(80);
//   IotWeb web;
//
//   IotWebConfig wc;
//   wc.user = WEB_USER;  wc.password = WEB_PASSWORD;  wc.realm = mqtt_client_id;
//   wc.status = statusJson;                    // String statusJson() { ... }
//   wc.deviceJs = DEVICE_DEVICE_JS;  wc.deviceJsLen = DEVICE_DEVICE_JS_LEN;
//   web.begin(server, wc);
//   web.addPage("/sim");                       // a page defined in device.js
//   server.on("/api/sim", HTTP_POST, [] { if (!web.requireAuth()) return; ...;
//                                         web.sendResult(200, true, "DONE", "..."); });
//
//   void loop() { server.handleClient(); }

#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <wifi_diag.h>

#include "iot_status.h"
#include "iot_web_assets.h"
#include "web_args.h"

struct IotWebConfig {
  const char* user = "admin";        // login for every write (POST) request
  const char* password = "";
  const char* realm = "esp32";       // name shown in the browser's login prompt (the device name)
  String (*status)() = nullptr;      // the device's /status JSON
  const uint8_t* deviceJs = nullptr;  // gzipped /device.js from tools/web_embed.py (nullptr: none)
  size_t deviceJsLen = 0;
};

class IotWeb {
 public:
  // Registers the library's pages and endpoints and starts the server. The
  // device's own pages (addPage) and endpoints can be added before or after.
  void begin(WebServer& server, const IotWebConfig& cfg) {
    server_ = &server;
    cfg_ = cfg;

    addPage("/");
    addPage("/wifi");
    server.on("/style.css", HTTP_GET, [this]() { sendGz(IOT_WEB_STYLE_CSS, IOT_WEB_STYLE_CSS_LEN, IOT_WEB_STYLE_CSS_TYPE); });
    server.on("/app.js", HTTP_GET, [this]() { sendGz(IOT_WEB_APP_JS, IOT_WEB_APP_JS_LEN, IOT_WEB_APP_JS_TYPE); });
    server.on("/device.js", HTTP_GET, [this]() {
      if (cfg_.deviceJs) {
        sendGz(cfg_.deviceJs, cfg_.deviceJsLen, "application/javascript");
      } else {
        server_->send(200, "application/javascript", "// no device script\n");
      }
    });
    server.on("/status", HTTP_GET, [this]() {
      if (cfg_.status) sendJson(200, cfg_.status());
      else sendResult(404, false, "", "No status");
    });
    addWifiApi();

    // ESP32 core 3.3 keeps no request headers unless asked to; without this,
    // server.authenticate() never sees the Authorization header.
    server.collectHeaders(nullptr, 0);
    server.begin();
  }

  // Serve the page shell at a path; the page itself is defined in device.js (IOT.page).
  void addPage(const char* path) {
    server_->on(path, HTTP_GET, [this]() { sendGz(IOT_WEB_PAGE_HTML, IOT_WEB_PAGE_HTML_LEN, IOT_WEB_PAGE_HTML_TYPE); });
  }

  // For every request that changes something: true if the login is right,
  // otherwise asks the browser for it (401) and returns false.
  bool requireAuth() {
    if (server_->authenticate(cfg_.user, cfg_.password)) return true;
    server_->requestAuthentication(BASIC_AUTH, cfg_.realm, "Authentication required");
    return false;
  }

  void sendJson(int code, const String& json) {
    server_->sendHeader("Cache-Control", "no-store");
    server_->send(code, "application/json", json);
  }

  // The answer the pages show in their message line: {"ok":..,"result":..,"detail":..}
  void sendResult(int code, bool ok, const String& result, const String& detail) {
    String j = "{\"ok\":" + String(ok ? "true" : "false");
    if (result.length()) j += ",\"result\":\"" + jsonEscape(result) + "\"";
    j += ",\"detail\":\"" + jsonEscape(detail) + "\"}";
    sendJson(code, j);
  }

  WebServer& server() { return *server_; }

 private:
  void sendGz(const uint8_t* data, size_t len, const char* type) {
    server_->sendHeader("Content-Encoding", "gzip");
    server_->sendHeader("Cache-Control", "no-cache");  // a new firmware brings new pages
    server_->send_P(200, type, (const char*)data, len);
  }

  // The WiFi page's endpoints (moved unchanged from the devices)
  void addWifiApi() {
    WebServer& server = *server_;
    server.on("/api/wifi", HTTP_GET, [this]() { sendJson(200, wifiDiag.statusJson(server_->hasArg("history"))); });

    server.on("/api/wifi/scan", HTTP_GET, [this]() {
      if (server_->hasArg("start") && !wifiDiag.startScan()) {
        sendJson(409, "{\"status\":\"busy\",\"detail\":\"A reception test is running\"}");
        return;
      }
      sendJson(200, wifiDiag.scanJson());
    });

    server.on("/api/wifi/test", HTTP_GET, [this]() { sendJson(200, wifiDiag.resultsJson()); });

    // Reception test: antenna (or label) and seconds, plus optional position,
    // x_ft, y_ft, height_ft, orientation, environment (indoor/outdoor) and note
    server.on("/api/wifi/test", HTTP_POST, [this]() {
      if (!requireAuth()) return;
      WebServer& s = *server_;
      long seconds = 60;
      if (s.hasArg("seconds") && !webParseLong(s.arg("seconds").c_str(), 10, WifiDiag::MAX_TEST_S, &seconds)) {
        sendResult(400, false, "", "seconds must be 10 to 600");
        return;
      }
      WifiTestMeta meta;
      String antenna = s.hasArg("antenna") ? s.arg("antenna") : s.arg("label");
      strncpy(meta.antenna, antenna.c_str(), sizeof(meta.antenna) - 1);
      strncpy(meta.position, s.arg("position").c_str(), sizeof(meta.position) - 1);
      strncpy(meta.orientation, s.arg("orientation").c_str(), sizeof(meta.orientation) - 1);
      strncpy(meta.environment, s.arg("environment").c_str(), sizeof(meta.environment) - 1);
      strncpy(meta.note, s.arg("note").c_str(), sizeof(meta.note) - 1);
      const char* numArgs[] = {"x_ft", "y_ft", "height_ft"};
      float* numDst[] = {&meta.xFt, &meta.yFt, &meta.heightFt};
      for (int k = 0; k < 3; k++) {
        String v = s.arg(numArgs[k]);
        v.trim();
        if (v.length() == 0) continue;
        if (!webParseFloat(v.c_str(), -100000, 100000, numDst[k])) {
          sendResult(400, false, "", String(numArgs[k]) + " must be a number (feet)");
          return;
        }
      }
      uint32_t id = wifiDiag.startTest(meta, (uint32_t)seconds);
      if (id == 0) {
        sendResult(409, false, "REJECTED", "A test is already running");
        return;
      }
      Serial.printf("Reception test %lu \"%s\" at \"%s\" started for %ld s\n", (unsigned long)id, meta.antenna,
                    meta.position, seconds);
      sendJson(200, "{\"ok\":true,\"result\":\"ACCEPTED\",\"id\":" + String(id) + ",\"detail\":\"Test started: " +
                        jsonEscape(wifiDiag.testLabel()) + "\"}");
    });

    server.on("/api/wifi/roam", HTTP_POST, [this]() {
      if (!requireAuth()) return;
      if (!wifiDiag.requestRoamCheck()) {
        sendResult(409, false, "REJECTED", "A reception test or a check is running");
        return;
      }
      sendResult(200, true, "ACCEPTED", "Checking for a stronger access point");
    });

    server.on("/api/wifi/config", HTTP_POST, [this]() {
      if (!requireAuth()) return;
      WebServer& s = *server_;
      String detail;
      if (s.hasArg("tx_power")) {
        float dbm = 0;
        if (!webParseFloat(s.arg("tx_power").c_str(), 2, 19.5, &dbm) || !WifiDiag::setTxPowerDbm(dbm)) {
          sendResult(400, false, "", "tx_power must be 2 to 19.5");
          return;
        }
        detail += "TX power " + String(WifiDiag::txPowerDbm(), 2) + " dBm. ";
      }
      if (s.hasArg("sleep")) {
        bool on = webParseBool(s.arg("sleep").c_str());
        WifiDiag::setModemSleep(on);
        detail += String("Modem sleep ") + (on ? "on." : "off.");
      }
      sendResult(200, true, "DONE", detail.length() ? detail : "Nothing changed");
    });
  }

  WebServer* server_ = nullptr;
  IotWebConfig cfg_;
};
