// web_args.h - checking numbers sent by the web pages (form fields, URL parameters).
//
// No Arduino dependencies: host-tested. Used by iot_web.h and by device endpoints:
//
//   long baud;
//   if (!webParseLong(server.arg("baud").c_str(), 1200, 115200, &baud)) { ...400... }

#pragma once

#include <cerrno>
#include <cmath>
#include <cstdlib>

// True if s is a whole number in [min, max] (spaces around it are allowed); stores it in *out.
inline bool webParseLong(const char* s, long min, long max, long* out) {
  if (!s) return false;
  while (*s == ' ') s++;
  if (!*s) return false;
  char* end = nullptr;
  errno = 0;
  long v = strtol(s, &end, 10);
  if (end == s || errno == ERANGE) return false;
  while (*end == ' ') end++;
  if (*end != '\0' || v < min || v > max) return false;
  *out = v;
  return true;
}

// True if s is a number in [min, max] (spaces around it are allowed); stores it in *out.
inline bool webParseFloat(const char* s, float min, float max, float* out) {
  if (!s) return false;
  while (*s == ' ') s++;
  if (!*s) return false;
  char* end = nullptr;
  float v = strtof(s, &end);
  if (end == s) return false;
  while (*end == ' ') end++;
  if (*end != '\0' || std::isnan(v) || v < min || v > max) return false;
  *out = v;
  return true;
}

// True for "1", "true" or "on" (a checkbox or switch sent by a form).
inline bool webParseBool(const char* s) {
  if (!s) return false;
  return (s[0] == '1' && s[1] == '\0') || (s[0] == 't' && s[1] == 'r' && s[2] == 'u' && s[3] == 'e' && s[4] == '\0') ||
         (s[0] == 'o' && s[1] == 'n' && s[2] == '\0');
}
