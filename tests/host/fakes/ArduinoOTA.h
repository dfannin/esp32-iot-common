// Host test stand-in for ArduinoOTA: records the setup and lets a test fire the callbacks.
#pragma once
#include <functional>
#include <string>

#define U_FLASH 0
#define U_SPIFFS 100

typedef enum {
  OTA_AUTH_ERROR,
  OTA_BEGIN_ERROR,
  OTA_CONNECT_ERROR,
  OTA_RECEIVE_ERROR,
  OTA_END_ERROR
} ota_error_t;

class FakeArduinoOTA {
 public:
  std::string hostname, password;
  int beginCalls = 0, handleCalls = 0, command = U_FLASH;
  std::function<void()> start, end;
  std::function<void(unsigned int, unsigned int)> progress;
  std::function<void(ota_error_t)> error;

  FakeArduinoOTA& setHostname(const char* h) { hostname = h; return *this; }
  FakeArduinoOTA& setPassword(const char* p) { password = p; return *this; }
  FakeArduinoOTA& onStart(std::function<void()> f) { start = f; return *this; }
  FakeArduinoOTA& onEnd(std::function<void()> f) { end = f; return *this; }
  FakeArduinoOTA& onProgress(std::function<void(unsigned int, unsigned int)> f) { progress = f; return *this; }
  FakeArduinoOTA& onError(std::function<void(ota_error_t)> f) { error = f; return *this; }
  int getCommand() { return command; }
  void begin() { beginCalls++; }
  void handle() { handleCalls++; }
};
extern FakeArduinoOTA ArduinoOTA;
