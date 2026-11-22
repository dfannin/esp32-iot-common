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
  String(const std::string& s) : s_(s) {}
  String(char c) : s_(1, c) {}
  String(int v) : s_(std::to_string(v)) {}
  String(unsigned int v) : s_(std::to_string(v)) {}
  String(long v) : s_(std::to_string(v)) {}
  String(unsigned long v) : s_(std::to_string(v)) {}
  String& operator=(const char* s) { s_ = s ? s : ""; return *this; }
  String& operator+=(const String& o) { s_ += o.s_; return *this; }
  String& operator+=(const char* o) { s_ += o ? o : ""; return *this; }
  String& operator+=(char c) { s_ += c; return *this; }
  bool operator==(const char* o) const { return s_ == (o ? o : ""); }
  bool operator==(const String& o) const { return s_ == o.s_; }
  const char* c_str() const { return s_.c_str(); }
  unsigned int length() const { return (unsigned int)s_.size(); }
  char charAt(unsigned int i) const { return i < s_.size() ? s_[i] : 0; }
  void reserve(unsigned int n) { s_.reserve(n); }
  int indexOf(const char* t) const { auto p = s_.find(t); return p == std::string::npos ? -1 : (int)p; }
  friend String operator+(const String& a, const String& b) { return String(a.s_ + b.s_); }
  friend String operator+(const String& a, const char* b) { return String(a.s_ + (b ? b : "")); }
  friend String operator+(const char* a, const String& b) { return String(std::string(a ? a : "") + b.s_); }
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

// CPU, heap and reset reason (esp32-hal-cpu.h, Esp.h, esp_system.h); set by the tests.
extern uint32_t fake_cpu_mhz;
inline uint32_t getCpuFrequencyMhz() { return fake_cpu_mhz; }
struct FakeEsp {
  uint32_t freeHeap = 0, minFreeHeap = 0;
  uint32_t getFreeHeap() { return freeHeap; }
  uint32_t getMinFreeHeap() { return minFreeHeap; }
};
extern FakeEsp ESP;
typedef enum {
  ESP_RST_UNKNOWN, ESP_RST_POWERON, ESP_RST_EXT, ESP_RST_SW, ESP_RST_PANIC, ESP_RST_INT_WDT,
  ESP_RST_TASK_WDT, ESP_RST_WDT, ESP_RST_DEEPSLEEP, ESP_RST_BROWNOUT, ESP_RST_SDIO
} esp_reset_reason_t;
