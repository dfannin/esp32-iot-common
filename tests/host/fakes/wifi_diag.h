// Host test stand-in for wifi_diag.h: records what the WiFi endpoints ask for.
#pragma once
#include <Arduino.h>

#include <cmath>
#include <string>

struct WifiTestMeta {
  char antenna[33] = "";
  char position[33] = "";
  char orientation[13] = "";
  char environment[9] = "";
  char note[65] = "";
  float xFt = NAN, yFt = NAN, heightFt = NAN;
};

class WifiDiag {
 public:
  static const int MAX_TEST_S = 600;
  bool busy = false;
  uint32_t tests = 0;
  WifiTestMeta lastMeta;
  uint32_t lastSeconds = 0;
  static float tx;
  static bool sleep;
  uint32_t startTest(const WifiTestMeta& m, uint32_t s) {
    if (busy) return 0;
    lastMeta = m; lastSeconds = s; busy = true;
    return ++tests;
  }
  const char* testLabel() const { return lastMeta.antenna; }
  bool requestRoamCheck() { return !busy; }
  bool startScan() { return !busy; }
  String scanJson() { return "{\"status\":\"done\",\"networks\":[]}"; }
  String statusJson(bool history) { return history ? "{\"history\":[]}" : "{}"; }
  String resultsJson() { return "{\"results\":[]}"; }
  static float txPowerDbm() { return tx; }
  static bool setTxPowerDbm(float d) { if (d < 2 || d > 19.5) return false; tx = d; return true; }
  static bool setModemSleep(bool on) { sleep = on; return true; }
};
extern WifiDiag wifiDiag;
