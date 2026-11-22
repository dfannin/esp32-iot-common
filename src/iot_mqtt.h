// iot_mqtt.h - MQTT over TLS with a last will and a reconnect schedule.
//
// Replaces the connect/retry code that each device had. The device keeps its
// topics and messages; it passes a function that runs after every connect
// (publish ONLINE, republish states, subscribe to commands).
//
// TLS only, with the broker's certificate checked against caCert: there is no
// plain-text and no "insecure" mode (iot-platform decision A9).
//
//   IotMqtt mqtt;
//   PubSubClient& client = mqtt.client();   // for publish(), subscribe(), connected()
//
//   void setup() {
//     IotMqttConfig cfg;
//     cfg.server = mqtt_server;  cfg.port = mqtt_port_ssl;  cfg.clientId = mqtt_client_id;
//     cfg.user = mqtt_user;  cfg.password = mqtt_password;  cfg.caCert = ca_cert;
//     cfg.willTopic = status_topic;  cfg.willPayload = offlineMessage.c_str();
//     mqtt.begin(cfg, mqttCallback, onMqttConnected);   // first attempt now
//   }
//   void loop() { mqtt.loop(); }                      // retries on the schedule

#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#include "retry_schedule.h"

struct IotMqttConfig {
  const char* server = nullptr;
  uint16_t port = 8883;
  const char* clientId = nullptr;  // also the mDNS / OTA host name on the devices
  const char* user = nullptr;
  const char* password = nullptr;
  const char* caCert = nullptr;  // PEM of the CA that signed the broker certificate (required)
  // Last will: published retained (QoS 1) by the broker if the device stops
  // responding. The payload is copied, so a temporary String is fine.
  const char* willTopic = nullptr;  // nullptr: no last will
  const char* willPayload = nullptr;
  uint16_t keepAliveSec = 60;
  uint16_t bufferSize = 2048;  // largest message (reception test results)
  uint32_t tlsTimeoutSec = 10;  // TLS handshake limit (WiFiClientSecure default is 120 s)
  RetrySchedule retry = {15000, 300000, 60000};  // 15 s for 5 min, then 60 s
};

class IotMqtt {
 public:
  using ConnectedFn = void (*)();

  // Configures the client and makes the first attempt if WiFi is connected.
  void begin(const IotMqttConfig& config, MQTT_CALLBACK_SIGNATURE, ConnectedFn onConnected);

  // Call every loop(): notices a lost connection, retries on the schedule while
  // WiFi is up, and runs PubSubClient's loop.
  void loop();

  // Next loop() makes an attempt at once (e.g. after roaming to another access point).
  void retryNow() { attempted_ = false; }

  PubSubClient& client() { return client_; }
  bool connected() { return client_.connected(); }

  uint32_t connects() const { return connects_; }           // successful connects since boot
  uint32_t failedAttempts() const { return failed_; }       // since the last successful connect
  const char* lastError() const { return lastError_; }      // "" until the first failure
  uint64_t lastErrorUptimeS() const { return lastErrorAt_; }  // uptime of the last failure
  uint32_t downForS();                                      // 0 while connected

 private:
  bool attempt();

  WiFiClientSecure tls_;
  PubSubClient client_{tls_};
  IotMqttConfig cfg_;
  String will_;
  ConnectedFn onConnected_ = nullptr;

  bool wasConnected_ = false;
  bool attempted_ = false;
  uint32_t lastAttempt_ = 0;
  uint32_t downSince_ = 0;
  uint32_t connects_ = 0;
  uint32_t failed_ = 0;
  char lastError_[112] = "";
  uint64_t lastErrorAt_ = 0;
};
