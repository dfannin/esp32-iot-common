// wifi_diag.h - WiFi and antenna diagnostics (design section 9.3)
//
// Samples RSSI once a second, pings the gateway once a second (ESP-IDF
// ping_sock), keeps a 10-minute RSSI history for the /wifi graph, rolling
// statistics for telemetry, and runs reception tests: each test records where
// it was taken (position, x/y/height in feet, antenna), the signal and latency
// statistics, disconnects and access-point changes during the test, and a scan
// of the access points and same-channel networks at the end.
//
// Roaming: the ESP32 keeps the access point it joined for as long as the signal
// is usable, even when a much stronger one of the same network comes back. Every
// ROAM_INTERVAL (2 h) the device scans, and if another access point of its
// network is at least ROAM_MARGIN_DB stronger, it reconnects (the connection
// joins the strongest access point). Never during a reception test.
//
// Part of esp32-iot-common, used by esp32-door, esp32-shed and esp32-garage.

#pragma once

#include <Arduino.h>

struct WifiStats {
  uint32_t samples = 0;  // RSSI samples
  float rssiAvg = 0;
  int rssiMin = 0;
  int rssiMax = 0;
  float rssiStd = 0;
  uint32_t pingSent = 0;
  uint32_t pingOk = 0;
  float pingAvgMs = 0;
  float pingMaxMs = 0;
  float pingLoss = 0;  // 0.0 - 1.0
};

// Where and how a test was taken (all optional except the antenna/label)
struct WifiTestMeta {
  char antenna[33] = "";      // antenna name (the test "label")
  char position[33] = "";     // e.g. "kitchen", "backyard-fence"
  char orientation[13] = "";  // "vertical", "horizontal", ...
  char environment[9] = "";   // "indoor" or "outdoor"
  char note[65] = "";
  float xFt = NAN;            // feet from the property origin (may be negative)
  float yFt = NAN;
  float heightFt = NAN;       // above the ground
};

// One access point of our own network (same SSID) seen by the end-of-test scan
struct OwnAp {
  char bssid[18] = "";
  int channel = 0;
  int rssi = 0;
};

struct WifiTestResult {
  uint32_t id = 0;
  WifiTestMeta meta;
  uint32_t seconds = 0;
  WifiStats stats;
  int rssiP10 = 0;          // the signal is at or above this 90 % of the time
  float pingP95Ms = 0;      // 95 % of the pings were faster than this
  uint32_t disconnects = 0;  // WiFi drops during the test
  uint32_t apChanges = 0;    // roams to another access point during the test
  char bssid[18] = "";       // access point at the end of the test
  int channel = 0;
  float txPowerDbm = 0;
  // End-of-test scan
  bool scanned = false;
  int networksSeen = 0;
  static const int MAX_OWN_APS = 6;
  OwnAp ownAps[MAX_OWN_APS];  // strongest first
  int ownApCount = 0;
  int sameChannelOthers = 0;        // other networks on our channel
  int sameChannelStrongest = 0;     // their strongest RSSI (0 if none)
  unsigned long epoch = 0;          // 0 if NTP was not synced
  uint64_t uptimeS = 0;
};

class WifiDiag {
 public:
  static const int HISTORY = 600;  // seconds of RSSI history
  static const int MAX_RESULTS = 10;
  static const int MAX_TEST_S = 600;

  // backgroundPing: ping the gateway all the time (rolling statistics for the
  // telemetry). false for battery-powered nodes: ping only during a test and
  // while someone looks at the WiFi page (for a minute after the last request).
  void begin(bool backgroundPing = true);
  void loop();

  // Rolling statistics since the last call (telemetry/garage/wifi)
  WifiStats takeWindow();

  // Reception test. Returns the test id (> 0), or 0 if one is already running.
  uint32_t startTest(const WifiTestMeta& meta, uint32_t seconds);
  bool testRunning() const { return phase_ != Phase::Idle; }
  bool testScanning() const { return phase_ == Phase::Scanning; }
  uint32_t testElapsedS() const;
  uint32_t testId() const { return testId_; }
  const char* testLabel() const { return meta_.antenna; }
  uint32_t testSeconds() const { return testSeconds_; }
  int resultCount() const { return resultCount_; }
  const WifiTestResult& result(int i) const;  // 0 = newest
  bool takeNewResult(WifiTestResult* out);     // for publishing, once per result

  // Settings (until the next restart)
  static float txPowerDbm();
  static bool setTxPowerDbm(float dbm);
  static bool setModemSleep(bool on);

  // RSSI history, oldest first; returns the number of samples copied
  int history(int8_t* out, int max) const;

  // Roaming (enabled by default)
  void setRoaming(bool enabled, uint32_t intervalMs, int marginDb);
  bool requestRoamCheck();  // scan now; false if a test or a check is running
  bool takeRoamed();        // true once after a roam: reconnect MQTT right away
  uint32_t roams() const { return roams_; }

  // Network scan (asynchronous); not while a test is running
  bool startScan();
  int scanState() const;  // -2 none, -1 running, >= 0 networks found
  String scanJson();

  String statusJson(bool includeHistory);
  String resultsJson();
  static String resultJson(const WifiTestResult& r);

 private:
  enum class Phase : uint8_t { Idle, Measuring, Scanning };

  struct Acc {
    uint32_t n = 0;
    double sum = 0, sumSq = 0;
    int min = 0, max = 0;
    uint32_t pingSent = 0, pingOk = 0;
    double pingSum = 0;
    float pingMax = 0;
    void addRssi(int rssi);
    void addPing(bool ok, float ms);
    WifiStats stats() const;
  };

  void startPing();
  void stopPing();
  static void onPingSuccess(void* hdl, void* args);
  static void onPingTimeout(void* hdl, void* args);
  void pingResult(bool ok, float ms);
  void finishMeasuring();
  void finishTest(bool scanOk);

  bool wantPing(bool connected) const;
  void startRoamScan();
  void evaluateRoam(int n);

  // Roaming
  bool roamEnabled_ = true;
  uint32_t roamIntervalMs_ = 2UL * 3600UL * 1000UL;
  int roamMarginDb_ = 8;
  bool roamScanning_ = false;
  bool roamRequested_ = false;
  uint32_t lastRoamCheck_ = 0;
  uint32_t roamScanStart_ = 0;
  uint32_t roamChecks_ = 0;
  uint32_t roams_ = 0;
  bool roamed_ = false;
  char roamResult_[96] = "No check yet";
  unsigned long lastRoamEpoch_ = 0;
  uint64_t lastRoamUptimeS_ = 0;

  bool backgroundPing_ = true;
  uint32_t lastInterest_ = 0;
  bool interest_ = false;
  void* ping_ = nullptr;
  uint32_t pingGateway_ = 0;
  uint32_t lastSample_ = 0;
  Acc window_;
  Acc test_;
  int8_t history_[HISTORY];
  int historyCount_ = 0;
  int historyHead_ = 0;

  // Test in progress
  Phase phase_ = Phase::Idle;
  WifiTestMeta meta_;
  uint32_t testId_ = 0;
  uint32_t nextId_ = 1;
  uint32_t testSeconds_ = 0;
  uint32_t testStart_ = 0;
  uint32_t scanStart_ = 0;
  int8_t testRssi_[MAX_TEST_S + 5];
  int testRssiN_ = 0;
  uint16_t testPing_[MAX_TEST_S + 5];  // ms, written by the ping task
  volatile int testPingN_ = 0;
  uint32_t testDisconnects_ = 0;
  uint32_t testApChanges_ = 0;
  bool wasConnected_ = false;
  char lastBssid_[18] = "";
  WifiTestResult pending_;  // filled when measuring ends, completed by the scan

  WifiTestResult results_[MAX_RESULTS];
  int resultCount_ = 0;
  bool newResult_ = false;
  bool scanStarted_ = false;
};

extern WifiDiag wifiDiag;
