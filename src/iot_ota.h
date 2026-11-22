// iot_ota.h - over-the-air updates from the Arduino tools, and mDNS.
//
// Replaces the setupOTA() and mDNS code that each device had. The device keeps
// its host name (the MQTT client id) and OTA password; it can pass a function
// that runs when an update starts (for example to release outputs first).
//
//   IotOta ota;
//
//   void setup() { ota.begin(mqtt_client_id, OTA_PASSWORD); }   // after WiFi
//   void loop()  { ota.loop(); }
//
// Upload: arduino-cli compile --upload -p <device IP> -l network .

#pragma once

#include <Arduino.h>

class IotOta {
 public:
  using StartFn = void (*)();

  // Starts mDNS as <hostname>.local with the "http" (port 80) and "ota"
  // (port 3232) services, then ArduinoOTA with the password. onStart, if given,
  // runs when an update begins, before anything is written.
  void begin(const char* hostname, const char* password, StartFn onStart = nullptr);

  // Call every loop(): answers OTA requests.
  void loop();

  bool mdnsStarted() const { return mdns_; }

 private:
  StartFn onStart_ = nullptr;
  bool mdns_ = false;
};
