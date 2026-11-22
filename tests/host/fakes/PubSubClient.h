// Host test stand-in for PubSubClient: connect() results are scripted.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include "WiFiClientSecure.h"
#define MQTT_CALLBACK_SIGNATURE std::function<void(char*, uint8_t*, unsigned int)> callback
struct PubSubClient {
  explicit PubSubClient(WiFiClientSecure& c) : tls(&c) {}
  WiFiClientSecure* tls;
  const char* server = nullptr; uint16_t port = 0; uint16_t buffer = 0; uint16_t keepalive = 0;
  bool hasCallback = false;
  bool up = false; int st = -1; bool nextConnectOk = false;
  int connectCalls = 0, loopCalls = 0;
  std::string lastUser, lastWillTopic, lastWill;
  void setServer(const char* s, uint16_t p) { server = s; port = p; }
  void setBufferSize(uint16_t b) { buffer = b; }
  void setKeepAlive(uint16_t k) { keepalive = k; }
  void setCallback(MQTT_CALLBACK_SIGNATURE) { hasCallback = (bool)callback; }
  bool connect(const char*, const char* user, const char*, const char* willTopic, uint8_t, bool,
               const char* will) {
    lastWillTopic = willTopic; lastWill = will; return doConnect(user);
  }
  bool connect(const char*, const char* user, const char*) { lastWillTopic = ""; return doConnect(user); }
  bool doConnect(const char* user) {
    connectCalls++; lastUser = user ? user : "";
    up = nextConnectOk; st = up ? 0 : -2; return up;
  }
  bool connected() { return up; }
  int state() { return st; }
  bool loop() { loopCalls++; return up; }
};
