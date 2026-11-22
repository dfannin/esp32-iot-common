#pragma once
#include <cstdio>
#include <cstring>
struct WiFiClientSecure {
  const char* ca = nullptr;
  unsigned long timeout = 120;
  int tlsCode = 0;
  const char* tlsText = "";
  void setCACert(const char* c) { ca = c; }
  void setHandshakeTimeout(unsigned long s) { timeout = s; }
  int lastError(char* buf, size_t n) { std::snprintf(buf, n, "%s", tlsText); return tlsCode; }
};
