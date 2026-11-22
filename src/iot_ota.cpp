// iot_ota.cpp - see iot_ota.h.

#include "iot_ota.h"

#include <ArduinoOTA.h>
#include <ESPmDNS.h>

void IotOta::begin(const char* hostname, const char* password, StartFn onStart) {
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

void IotOta::loop() { ArduinoOTA.handle(); }
