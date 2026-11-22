// Host test stand-in for the parts of Arduino.h that the library uses.
#pragma once
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

extern uint32_t fake_millis;
inline uint32_t millis() { return fake_millis; }

class String {
 public:
  String() {}
  String(const char* s) : s_(s ? s : "") {}
  String& operator=(const char* s) { s_ = s ? s : ""; return *this; }
  const char* c_str() const { return s_.c_str(); }
 private:
  std::string s_;
};

struct FakeSerial {
  template <typename... A> void printf(const char* f, A... a) { std::printf(f, a...); }
  void print(const char* s) { std::printf("%s", s); }
  void println(const char* s = "") { std::printf("%s\n", s); }
};
extern FakeSerial Serial;

// Time functions of the ESP32 core (esp32-hal-time); defined by the tests that use them.
void configTzTime(const char* tz, const char* server1, const char* server2 = nullptr,
                  const char* server3 = nullptr);
bool getLocalTime(struct tm* info, uint32_t ms = 5000);
