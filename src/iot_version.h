// iot_version.h - firmware version helpers (iot-platform architecture §6.4).
//
// A device sketch includes its generated version.h (if present) BEFORE the
// library, then uses FW_VERSION and FW_BUILD:
//
//   #if __has_include("version.h")
//   #include "version.h"       // written by tools/fw_version.sh, not in git
//   #endif
//   #include <esp32_iot_common.h>
//
//   const char* fversion = FW_VERSION;   // "261122-1530", "...-dirty" or "...-local"
//   const char* fbuild = FW_BUILD;       // short commit hash, "" for -local
//
// Without version.h (Arduino IDE, zip download) FW_VERSION falls back to the
// compiler's build time in local time with "-local", and FW_BUILD is empty.
// No Arduino dependencies: host-tested.

#pragma once

struct IotVersionText {
  char text[18];  // "YYMMDD-HHMM-local" + NUL
};

// Month number from __DATE__ ("Nov 22 2026"); 0 if unknown.
constexpr int iotMonthFromDate(const char* d) {
  const char* names = "JanFebMarAprMayJunJulAugSepOctNovDec";
  for (int m = 0; m < 12; m++) {
    if (d[0] == names[3 * m] && d[1] == names[3 * m + 1] && d[2] == names[3 * m + 2]) return m + 1;
  }
  return 0;
}

// "YYMMDD-HHMM-local" from __DATE__ ("Nov  2 2026") and __TIME__ ("15:30:12").
constexpr IotVersionText iotLocalVersion(const char* date, const char* time) {
  IotVersionText v{};
  int month = iotMonthFromDate(date);
  v.text[0] = date[9];
  v.text[1] = date[10];
  v.text[2] = (char)('0' + month / 10);
  v.text[3] = (char)('0' + month % 10);
  v.text[4] = date[4] == ' ' ? '0' : date[4];
  v.text[5] = date[5];
  v.text[6] = '-';
  v.text[7] = time[0];
  v.text[8] = time[1];
  v.text[9] = time[3];
  v.text[10] = time[4];
  const char* suffix = "-local";
  for (int i = 0; i < 6; i++) v.text[11 + i] = suffix[i];
  v.text[17] = '\0';
  return v;
}

// True if v follows the convention: YYMMDD-HHMM, optionally -dirty or -local.
constexpr bool iotVersionValid(const char* v) {
  for (int i = 0; i < 11; i++) {
    if (i == 6) {
      if (v[i] != '-') return false;
    } else if (v[i] < '0' || v[i] > '9') {
      return false;
    }
  }
  if (v[11] == '\0') return true;
  const char* dirty = "-dirty";
  const char* local = "-local";
  bool isDirty = true, isLocal = true;
  for (int i = 0; i < 6; i++) {
    if (v[11 + i] != dirty[i]) isDirty = false;
    if (v[11 + i] != local[i]) isLocal = false;
    if (v[11 + i] == '\0') return false;
  }
  return (isDirty || isLocal) && v[17] == '\0';
}

#ifndef FW_VERSION
static constexpr IotVersionText kIotLocalVersion = iotLocalVersion(__DATE__, __TIME__);
#define FW_VERSION (kIotLocalVersion.text)
#endif

#ifndef FW_BUILD
#define FW_BUILD ""
#endif
