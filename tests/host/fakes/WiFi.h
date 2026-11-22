// Host test stand-in for the parts of WiFi.h that the library uses.
#pragma once
#include <Arduino.h>

enum { WL_CONNECTED = 3, WL_DISCONNECTED = 6 };

struct FakeIP {
  String text;
  String toString() const { return text; }
};

struct FakeWiFi {
  int s = WL_CONNECTED;
  String mac = "00:00:00:00:00:00", bssid = "00:00:00:00:00:00";
  FakeIP ip{"0.0.0.0"};
  int rssi = 0, ch = 0;
  int status() { return s; }
  String macAddress() { return mac; }
  FakeIP localIP() { return ip; }
  int RSSI() { return rssi; }
  String BSSIDstr() { return bssid; }
  int channel() { return ch; }
};
extern FakeWiFi WiFi;
