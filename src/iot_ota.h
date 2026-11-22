// iot_ota.h - over-the-air updates from the Arduino tools, and mDNS.
//
// Opt-in: this header is NOT included by esp32_iot_common.h. A sketch that
// wants OTA includes it itself, so a sketch without OTA does not link the
// core's ArduinoOTA and mDNS code (about 40 KB of flash). It is header-only
// for the same reason: no library .cpp file pulls those libraries in.
//
//   #include <esp32_iot_common.h>
//   #include <iot_ota.h>
//
//   IotOta ota;
//
//   void setup() { ota.begin(mqtt_client_id, OTA_PASSWORD); }   // after WiFi
//   void loop()  { ota.loop(); }
//
// The device keeps its host name (the MQTT client id) and OTA password; it can
// pass a function that runs when an update starts (for example to release
// outputs first).
//
// Upload: arduino-cli compile --upload -p <device IP> -l network .

#pragma once

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

class IotOta {
 public:
  using StartFn = void (*)();

  // Starts mDNS as <hostname>.local with the "http" (port 80) and "ota"
  // (port 3232) services, then ArduinoOTA with the password. onStart, if given,
  // runs when an update begins, before anything is written.
  void begin(const char* hostname, const char* password, StartFn onStart = nullptr) {
    onStart_ = onStart;

    mdns_ = MDNS.begin(hostname);
    if (mdns_) {
      MDNS.addService("http", "tcp", 80);
      MDNS.addService("ota", "tcp", 3232);
      Serial.println("MDNS responder started");
    } else {
      Serial.println("Error setting up MDNS responder!");
    }

    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.setPassword(password);

    ArduinoOTA.onStart([this]() {
      Serial.println(ArduinoOTA.getCommand() == U_FLASH ? "Start updating sketch"
                                                        : "Start updating filesystem");
      if (onStart_) onStart_();
    });
    ArduinoOTA.onEnd([]() { Serial.println("\nOTA Update Complete!"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", total ? (unsigned)((uint64_t)progress * 100 / total) : 0u);
    });
    ArduinoOTA.onError([](ota_error_t error) {
      const char* text = error == OTA_AUTH_ERROR      ? "Auth Failed"
                         : error == OTA_BEGIN_ERROR   ? "Begin Failed"
                         : error == OTA_CONNECT_ERROR ? "Connect Failed"
                         : error == OTA_RECEIVE_ERROR ? "Receive Failed"
                         : error == OTA_END_ERROR     ? "End Failed"
                                                      : "Unknown";
      Serial.printf("OTA Error[%u]: %s\n", (unsigned)error, text);
    });

    ArduinoOTA.begin();
    Serial.println("OTA Ready");
  }

  // Call every loop(): answers OTA requests.
  void loop() { ArduinoOTA.handle(); }

  bool mdnsStarted() const { return mdns_; }

 private:
  StartFn onStart_ = nullptr;
  bool mdns_ = false;
};
