#pragma once
enum { WL_CONNECTED = 3, WL_DISCONNECTED = 6 };
struct FakeWiFi { int s = WL_CONNECTED; int status() { return s; } };
extern FakeWiFi WiFi;
