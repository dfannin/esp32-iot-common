// iot_mqtt.cpp - see iot_mqtt.h

#include "iot_mqtt.h"

#include <WiFi.h>
#include <esp_timer.h>

void IotMqtt::begin(const IotMqttConfig& config, MQTT_CALLBACK_SIGNATURE, ConnectedFn onConnected) {
  cfg_ = config;
  will_ = config.willPayload ? config.willPayload : "";
  cfg_.willPayload = nullptr;  // use will_ (the caller's string may be temporary)
  onConnected_ = onConnected;

  tls_.setCACert(cfg_.caCert);
  tls_.setHandshakeTimeout(cfg_.tlsTimeoutSec);
  client_.setServer(cfg_.server, cfg_.port);
  client_.setBufferSize(cfg_.bufferSize);
  client_.setKeepAlive(cfg_.keepAliveSec);
  client_.setCallback(callback);

  downSince_ = millis();  // if the first attempt fails, retry quickly
  if (WiFi.status() == WL_CONNECTED) attempt();
}

void IotMqtt::loop() {
  bool up = client_.connected();
  if (wasConnected_ && !up) {
    Serial.println("MQTT connection lost");
    downSince_ = millis();
    attempted_ = false;  // first retry right away
  }
  wasConnected_ = up;
  if (!up && WiFi.status() == WL_CONNECTED &&
      cfg_.retry.due(millis(), downSince_, lastAttempt_, attempted_)) {
    attempt();
  }
  client_.loop();
}

uint32_t IotMqtt::downForS() {
  return client_.connected() ? 0 : (millis() - downSince_) / 1000;
}

// One TLS attempt; it blocks for at most about tlsTimeoutSec.
bool IotMqtt::attempt() {
  attempted_ = true;
  lastAttempt_ = millis();
  Serial.printf("MQTT: connecting to %s:%u (TLS) as %s...", cfg_.server, cfg_.port, cfg_.clientId);

  bool ok = cfg_.willTopic
                ? client_.connect(cfg_.clientId, cfg_.user, cfg_.password, cfg_.willTopic, 1, true,
                                  will_.c_str())
                : client_.connect(cfg_.clientId, cfg_.user, cfg_.password);
  if (ok) {
    connects_++;
    failed_ = 0;
    wasConnected_ = true;
    Serial.println(" connected");
    if (onConnected_) onConnected_();
    return true;
  }

  failed_++;
  int state = client_.state();
  char tlsError[64] = "";
  int tlsCode = tls_.lastError(tlsError, sizeof(tlsError));
  if (tlsCode != 0) {
    snprintf(lastError_, sizeof(lastError_), "%s (rc=%d), TLS %d: %s", mqttStateText(state), state,
             tlsCode, tlsError);
  } else {
    snprintf(lastError_, sizeof(lastError_), "%s (rc=%d)", mqttStateText(state), state);
  }
  lastErrorAt_ = (uint64_t)(esp_timer_get_time() / 1000000);
  Serial.printf(" failed: %s; retry in %lu s\n", lastError_,
                (unsigned long)(cfg_.retry.intervalMs(millis(), downSince_) / 1000));
  return false;
}
