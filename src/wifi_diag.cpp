// wifi_diag.cpp - WiFi and antenna diagnostics

#include "wifi_diag.h"

#include <WiFi.h>
#include <esp_timer.h>
#include <math.h>
#include <time.h>

#include "lwip/ip_addr.h"
#include "ping/ping_sock.h"
#include "stats_util.h"

WifiDiag wifiDiag;

static portMUX_TYPE pingMux = portMUX_INITIALIZER_UNLOCKED;

// Transmit power steps the ESP32 driver accepts (wifi_power_t is in 0.25 dBm)
static const wifi_power_t TX_STEPS[] = {
    WIFI_POWER_19_5dBm, WIFI_POWER_19dBm, WIFI_POWER_18_5dBm, WIFI_POWER_17dBm,
    WIFI_POWER_15dBm,   WIFI_POWER_13dBm, WIFI_POWER_11dBm,   WIFI_POWER_8_5dBm,
    WIFI_POWER_7dBm,    WIFI_POWER_5dBm,  WIFI_POWER_2dBm};

// ---------------------------------------------------------------------------
// Accumulator
// ---------------------------------------------------------------------------

void WifiDiag::Acc::addRssi(int rssi) {
  if (n == 0) {
    min = max = rssi;
  } else {
    if (rssi < min) min = rssi;
    if (rssi > max) max = rssi;
  }
  n++;
  sum += rssi;
  sumSq += (double)rssi * rssi;
}

void WifiDiag::Acc::addPing(bool ok, float ms) {
  pingSent++;
  if (ok) {
    pingOk++;
    pingSum += ms;
    if (ms > pingMax) pingMax = ms;
  }
}

WifiStats WifiDiag::Acc::stats() const {
  WifiStats s;
  s.samples = n;
  if (n > 0) {
    s.rssiAvg = sum / n;
    s.rssiMin = min;
    s.rssiMax = max;
    double var = sumSq / n - (sum / n) * (sum / n);
    s.rssiStd = var > 0 ? sqrt(var) : 0;
  }
  s.pingSent = pingSent;
  s.pingOk = pingOk;
  s.pingAvgMs = pingOk ? pingSum / pingOk : 0;
  s.pingMaxMs = pingMax;
  s.pingLoss = pingSent ? 1.0f - (float)pingOk / pingSent : 0;
  return s;
}

// ---------------------------------------------------------------------------
// Ping (ESP-IDF ping_sock: runs in its own task, results come by callback)
// ---------------------------------------------------------------------------

void WifiDiag::onPingSuccess(void* hdl, void* args) {
  uint32_t elapsed = 0;
  esp_ping_get_profile((esp_ping_handle_t)hdl, ESP_PING_PROF_TIMEGAP, &elapsed, sizeof(elapsed));
  ((WifiDiag*)args)->pingResult(true, (float)elapsed);
}

void WifiDiag::onPingTimeout(void* hdl, void* args) {
  (void)hdl;
  ((WifiDiag*)args)->pingResult(false, 0);
}

void WifiDiag::pingResult(bool ok, float ms) {
  portENTER_CRITICAL(&pingMux);
  window_.addPing(ok, ms);
  if (phase_ == Phase::Measuring) {
    test_.addPing(ok, ms);
    if (ok && testPingN_ < MAX_TEST_S + 5) {
      testPing_[testPingN_] = ms > 65535 ? 65535 : (uint16_t)ms;
      testPingN_ = testPingN_ + 1;
    }
  }
  portEXIT_CRITICAL(&pingMux);
}

void WifiDiag::startPing() {
  IPAddress gw = WiFi.gatewayIP();
  uint32_t gwKey = ((uint32_t)gw[0] << 24) | ((uint32_t)gw[1] << 16) | ((uint32_t)gw[2] << 8) | gw[3];
  if (gwKey == 0) return;

  esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
  ip_addr_t target;
  memset(&target, 0, sizeof(target));
  IP_ADDR4(&target, gw[0], gw[1], gw[2], gw[3]);
  config.target_addr = target;
  config.count = ESP_PING_COUNT_INFINITE;
  config.interval_ms = 1000;
  config.timeout_ms = 1000;

  esp_ping_callbacks_t cbs;
  memset(&cbs, 0, sizeof(cbs));
  cbs.cb_args = this;
  cbs.on_ping_success = (void (*)(esp_ping_handle_t, void*))onPingSuccess;
  cbs.on_ping_timeout = (void (*)(esp_ping_handle_t, void*))onPingTimeout;

  esp_ping_handle_t handle = nullptr;
  if (esp_ping_new_session(&config, &cbs, &handle) == ESP_OK) {
    esp_ping_start(handle);
    ping_ = handle;
    pingGateway_ = gwKey;
  }
}

void WifiDiag::stopPing() {
  if (ping_ == nullptr) return;
  esp_ping_stop((esp_ping_handle_t)ping_);
  esp_ping_delete_session((esp_ping_handle_t)ping_);
  ping_ = nullptr;
  pingGateway_ = 0;
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

void WifiDiag::begin(bool backgroundPing) {
  backgroundPing_ = backgroundPing;
  lastSample_ = millis();
  lastRoamCheck_ = millis();  // the connection at boot already chose the strongest
}

// ---------------------------------------------------------------------------
// Roaming
// ---------------------------------------------------------------------------

void WifiDiag::setRoaming(bool enabled, uint32_t intervalMs, int marginDb) {
  roamEnabled_ = enabled;
  if (intervalMs >= 60000) roamIntervalMs_ = intervalMs;
  if (marginDb >= 3) roamMarginDb_ = marginDb;
}

bool WifiDiag::requestRoamCheck() {
  if (phase_ != Phase::Idle || roamScanning_ || WiFi.status() != WL_CONNECTED) return false;
  roamRequested_ = true;
  return true;
}

bool WifiDiag::takeRoamed() {
  bool r = roamed_;
  roamed_ = false;
  return r;
}

void WifiDiag::startRoamScan() {
  if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) return;  // a /wifi scan: try again next loop
  WiFi.scanDelete();
  if (WiFi.scanNetworks(true, true) == WIFI_SCAN_FAILED) {
    lastRoamCheck_ = millis();
    snprintf(roamResult_, sizeof(roamResult_), "Scan failed");
    return;
  }
  roamScanning_ = true;
  roamRequested_ = false;
  roamScanStart_ = millis();
  scanStarted_ = true;  // the results also show under Nearby networks
}

void WifiDiag::evaluateRoam(int n) {
  roamScanning_ = false;
  lastRoamCheck_ = millis();
  roamChecks_++;
  String ownSsid = WiFi.SSID();
  String cur = WiFi.BSSIDstr();
  int curRssi = WiFi.RSSI();
  int bestRssi = 0;
  String best;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) != ownSsid) continue;
    if (WiFi.BSSIDstr(i) == cur) {
      curRssi = WiFi.RSSI(i);  // compare scan with scan
      continue;
    }
    if (bestRssi == 0 || WiFi.RSSI(i) > bestRssi) {
      bestRssi = WiFi.RSSI(i);
      best = WiFi.BSSIDstr(i);
    }
  }
  if (!shouldRoam(curRssi, bestRssi, roamMarginDb_)) {
    if (bestRssi == 0) {
      snprintf(roamResult_, sizeof(roamResult_), "Stayed: %s %d dBm, no other access point",
               cur.c_str() + 9, curRssi);
    } else {
      snprintf(roamResult_, sizeof(roamResult_), "Stayed: %s %d dBm, best other %s %d dBm",
               cur.c_str() + 9, curRssi, best.c_str() + 9, bestRssi);
    }
    return;
  }
  if (phase_ != Phase::Idle) {
    snprintf(roamResult_, sizeof(roamResult_), "Not moved (test running): %s %d dBm is stronger",
             best.c_str() + 9, bestRssi);
    return;
  }
  snprintf(roamResult_, sizeof(roamResult_), "Moved: %s %d dBm -> %s %d dBm", cur.c_str() + 9, curRssi,
           best.c_str() + 9, bestRssi);
  Serial.printf("WiFi roaming: %s (%d dBm) -> %s (%d dBm)\n", cur.c_str(), curRssi, best.c_str(), bestRssi);
  roams_++;
  roamed_ = true;
  time_t t = time(nullptr);
  lastRoamEpoch_ = t > 1700000000 ? (unsigned long)t : 0;
  lastRoamUptimeS_ = (uint64_t)esp_timer_get_time() / 1000000ULL;
  // Reconnect with the stored settings: all-channel scan, strongest access point
  WiFi.reconnect();
}

bool WifiDiag::wantPing(bool connected) const {
  if (!connected) return false;
  if (backgroundPing_ || phase_ == Phase::Measuring) return true;
  return interest_ && millis() - lastInterest_ < 60000;  // the WiFi page is open
}

void WifiDiag::loop() {
  bool connected = WiFi.status() == WL_CONNECTED;

  // Keep one ping session to the current gateway while connected
  if (wantPing(connected)) {
    IPAddress gw = WiFi.gatewayIP();
    uint32_t gwKey = ((uint32_t)gw[0] << 24) | ((uint32_t)gw[1] << 16) | ((uint32_t)gw[2] << 8) | gw[3];
    if (ping_ != nullptr && gwKey != pingGateway_) stopPing();
    if (ping_ == nullptr) startPing();
  } else if (ping_ != nullptr) {
    stopPing();
  }

  // Disconnects during a test
  if (phase_ == Phase::Measuring) {
    if (wasConnected_ && !connected) testDisconnects_++;
    wasConnected_ = connected;
  }

  uint32_t now = millis();
  if (now - lastSample_ >= 1000) {
    lastSample_ += 1000;
    if (now - lastSample_ >= 1000) lastSample_ = now;  // fell behind: don't catch up
    if (connected) {
      int rssi = WiFi.RSSI();
      history_[historyHead_] = (int8_t)rssi;
      historyHead_ = (historyHead_ + 1) % HISTORY;
      if (historyCount_ < HISTORY) historyCount_++;
      portENTER_CRITICAL(&pingMux);
      window_.addRssi(rssi);
      if (phase_ == Phase::Measuring) test_.addRssi(rssi);
      portEXIT_CRITICAL(&pingMux);

      if (phase_ == Phase::Measuring) {
        if (testRssiN_ < MAX_TEST_S + 5) testRssi_[testRssiN_++] = (int8_t)rssi;
        // Roaming to another access point of the mesh
        String bssid = WiFi.BSSIDstr();
        if (lastBssid_[0] != '\0' && strcmp(lastBssid_, bssid.c_str()) != 0) testApChanges_++;
        strncpy(lastBssid_, bssid.c_str(), sizeof(lastBssid_) - 1);
      }
    }
  }

  if (phase_ == Phase::Measuring && testElapsedS() >= testSeconds_) finishMeasuring();

  // Roaming check every roamIntervalMs_, or when requested from the WiFi page
  if (!roamScanning_ && phase_ == Phase::Idle && connected &&
      (roamRequested_ || (roamEnabled_ && millis() - lastRoamCheck_ >= roamIntervalMs_))) {
    startRoamScan();
  }
  if (roamScanning_) {
    int n = WiFi.scanComplete();
    if (n >= 0) {
      evaluateRoam(n);
    } else if (n != WIFI_SCAN_RUNNING || millis() - roamScanStart_ > 15000) {
      roamScanning_ = false;
      lastRoamCheck_ = millis();
      snprintf(roamResult_, sizeof(roamResult_), "Scan failed");
    }
  }

  if (phase_ == Phase::Scanning) {
    int n = WiFi.scanComplete();
    if (n >= 0) {
      finishTest(true);
    } else if (n != WIFI_SCAN_RUNNING || millis() - scanStart_ > 15000) {
      finishTest(false);
    }
  }
}

WifiStats WifiDiag::takeWindow() {
  portENTER_CRITICAL(&pingMux);
  WifiStats s = window_.stats();
  window_ = Acc();
  portEXIT_CRITICAL(&pingMux);
  return s;
}

int WifiDiag::history(int8_t* out, int max) const {
  int n = historyCount_ < max ? historyCount_ : max;
  int start = (historyHead_ - n + HISTORY) % HISTORY;
  for (int i = 0; i < n; i++) out[i] = history_[(start + i) % HISTORY];
  return n;
}

// ---------------------------------------------------------------------------
// Reception test
// ---------------------------------------------------------------------------

uint32_t WifiDiag::startTest(const WifiTestMeta& meta, uint32_t seconds) {
  if (phase_ != Phase::Idle || roamScanning_) return 0;
  if (seconds < 10) seconds = 10;
  if (seconds > MAX_TEST_S) seconds = MAX_TEST_S;
  meta_ = meta;
  if (meta_.antenna[0] == '\0') strcpy(meta_.antenna, "unnamed");
  testSeconds_ = seconds;
  testStart_ = millis();
  testId_ = nextId_++;
  testRssiN_ = 0;
  testDisconnects_ = 0;
  testApChanges_ = 0;
  wasConnected_ = WiFi.status() == WL_CONNECTED;
  strncpy(lastBssid_, wasConnected_ ? WiFi.BSSIDstr().c_str() : "", sizeof(lastBssid_) - 1);
  lastBssid_[sizeof(lastBssid_) - 1] = '\0';
  portENTER_CRITICAL(&pingMux);
  test_ = Acc();
  testPingN_ = 0;
  phase_ = Phase::Measuring;
  portEXIT_CRITICAL(&pingMux);
  return testId_;
}

uint32_t WifiDiag::testElapsedS() const {
  return phase_ == Phase::Idle ? 0 : (millis() - testStart_) / 1000;
}

// Measuring done: compute the statistics, then scan the neighbourhood
void WifiDiag::finishMeasuring() {
  portENTER_CRITICAL(&pingMux);
  WifiStats s = test_.stats();
  phase_ = Phase::Scanning;  // no more samples go into the test
  int pingN = testPingN_;
  portEXIT_CRITICAL(&pingMux);

  WifiTestResult& r = pending_;
  r = WifiTestResult();
  r.id = testId_;
  r.meta = meta_;
  r.seconds = testSeconds_;
  r.stats = s;
  r.rssiP10 = percentile<int8_t>(testRssi_, testRssiN_, 10);
  r.pingP95Ms = percentile<uint16_t>(testPing_, pingN, 95);
  r.disconnects = testDisconnects_;
  r.apChanges = testApChanges_;
  strncpy(r.bssid, WiFi.BSSIDstr().c_str(), sizeof(r.bssid) - 1);
  r.channel = WiFi.channel();
  r.txPowerDbm = txPowerDbm();
  time_t now = time(nullptr);
  r.epoch = now > 1700000000 ? (unsigned long)now : 0;
  r.uptimeS = (uint64_t)esp_timer_get_time() / 1000000ULL;

  scanStart_ = millis();
  if (WiFi.status() != WL_CONNECTED) {
    finishTest(false);
    return;
  }
  if (WiFi.scanComplete() != WIFI_SCAN_RUNNING) {  // a /wifi scan may already be running: use it
    WiFi.scanDelete();
    if (WiFi.scanNetworks(true, true) == WIFI_SCAN_FAILED) {
      finishTest(false);
      return;
    }
  }
  scanStarted_ = true;
}

void WifiDiag::finishTest(bool scanOk) {
  WifiTestResult& r = pending_;
  if (scanOk) {
    int n = WiFi.scanComplete();
    String ownSsid = WiFi.SSID();
    r.scanned = true;
    r.networksSeen = n;
    for (int i = 0; i < n; i++) {
      int rssi = WiFi.RSSI(i);
      if (WiFi.SSID(i) == ownSsid) {
        // Keep the strongest MAX_OWN_APS, strongest first
        int pos = r.ownApCount;
        while (pos > 0 && r.ownAps[pos - 1].rssi < rssi) pos--;
        if (pos >= WifiTestResult::MAX_OWN_APS) continue;
        int last = r.ownApCount < WifiTestResult::MAX_OWN_APS ? r.ownApCount : WifiTestResult::MAX_OWN_APS - 1;
        for (int k = last; k > pos; k--) r.ownAps[k] = r.ownAps[k - 1];
        OwnAp& ap = r.ownAps[pos];
        ap = OwnAp();
        strncpy(ap.bssid, WiFi.BSSIDstr(i).c_str(), sizeof(ap.bssid) - 1);
        ap.channel = WiFi.channel(i);
        ap.rssi = rssi;
        if (r.ownApCount < WifiTestResult::MAX_OWN_APS) r.ownApCount++;
      } else if (WiFi.channel(i) == r.channel) {
        if (r.sameChannelOthers == 0 || rssi > r.sameChannelStrongest) r.sameChannelStrongest = rssi;
        r.sameChannelOthers++;
      }
    }
  }

  for (int i = MAX_RESULTS - 1; i > 0; i--) results_[i] = results_[i - 1];
  results_[0] = r;
  if (resultCount_ < MAX_RESULTS) resultCount_++;
  newResult_ = true;
  phase_ = Phase::Idle;
}

const WifiTestResult& WifiDiag::result(int i) const { return results_[i]; }

bool WifiDiag::takeNewResult(WifiTestResult* out) {
  if (!newResult_ || resultCount_ == 0) return false;
  newResult_ = false;
  *out = results_[0];
  return true;
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

float WifiDiag::txPowerDbm() { return (int)WiFi.getTxPower() / 4.0f; }

bool WifiDiag::setTxPowerDbm(float dbm) {
  if (dbm < 2 || dbm > 19.5) return false;
  // Highest step that does not exceed the request
  wifi_power_t best = WIFI_POWER_2dBm;
  for (wifi_power_t p : TX_STEPS) {
    if ((int)p <= (int)(dbm * 4 + 0.01f)) {
      best = p;
      break;
    }
  }
  return WiFi.setTxPower(best);
}

bool WifiDiag::setModemSleep(bool on) { return WiFi.setSleep(on); }

// ---------------------------------------------------------------------------
// Scan
// ---------------------------------------------------------------------------

bool WifiDiag::startScan() {
  if (phase_ != Phase::Idle || roamScanning_) return false;  // the test / roam check scans itself
  if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) return true;
  WiFi.scanDelete();
  WiFi.scanNetworks(true, true);  // async, include hidden networks
  scanStarted_ = true;
  return true;
}

int WifiDiag::scanState() const {
  if (!scanStarted_) return -2;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return -1;
  return n < 0 ? -2 : n;
}

static const char* authName(wifi_auth_mode_t m) {
  switch (m) {
    case WIFI_AUTH_OPEN:            return "open";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-EAP";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
    default:                        return "other";
  }
}

static String jsonStr(const String& in) {
  String out = "\"";
  for (unsigned int i = 0; i < in.length(); i++) {
    char c = in.charAt(i);
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((uint8_t)c < 0x20) {
      out += ' ';
    } else {
      out += c;
    }
  }
  return out + "\"";
}

static String jsonNum(float v, int decimals) { return isnan(v) ? String("null") : String(v, decimals); }

String WifiDiag::scanJson() {
  int state = scanState();
  if (phase_ == Phase::Scanning || roamScanning_ || state == -1) return "{\"status\":\"scanning\"}";
  if (state == -2) return "{\"status\":\"none\"}";
  String json = "{\"status\":\"done\",\"networks\":[";
  for (int i = 0; i < state; i++) {
    if (i > 0) json += ",";
    json += "{\"ssid\":" + jsonStr(WiFi.SSID(i));
    json += ",\"bssid\":\"" + WiFi.BSSIDstr(i) + "\"";
    json += ",\"channel\":" + String(WiFi.channel(i));
    json += ",\"rssi\":" + String(WiFi.RSSI(i));
    json += ",\"auth\":\"" + String(authName(WiFi.encryptionType(i))) + "\"";
    json += ",\"connected\":" + String(WiFi.BSSIDstr(i) == WiFi.BSSIDstr() ? "true" : "false") + "}";
  }
  return json + "]}";
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------

static String statsJson(const WifiStats& s) {
  String j = "\"samples\":" + String(s.samples);
  j += ",\"rssi\":" + String(s.rssiAvg, 1);
  j += ",\"rssi_min\":" + String(s.rssiMin);
  j += ",\"rssi_max\":" + String(s.rssiMax);
  j += ",\"rssi_std\":" + String(s.rssiStd, 2);
  j += ",\"ping_sent\":" + String(s.pingSent);
  j += ",\"ping_ms\":" + String(s.pingAvgMs, 1);
  j += ",\"ping_ms_max\":" + String(s.pingMaxMs, 1);
  j += ",\"ping_loss\":" + String(s.pingLoss, 3);
  return j;
}

String WifiDiag::statusJson(bool includeHistory) {
  interest_ = true;
  lastInterest_ = millis();
  bool connected = WiFi.status() == WL_CONNECTED;
  String j = "{\"connected\":" + String(connected ? "true" : "false");
  j += ",\"ssid\":" + jsonStr(WiFi.SSID());
  j += ",\"bssid\":\"" + WiFi.BSSIDstr() + "\"";
  j += ",\"channel\":" + String(WiFi.channel());
  j += ",\"rssi\":" + String(connected ? WiFi.RSSI() : 0);
  j += ",\"tx_power_dbm\":" + String(txPowerDbm(), 2);
  j += ",\"modem_sleep\":" + String(WiFi.getSleep() ? "true" : "false");
  j += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  j += ",\"gateway\":\"" + WiFi.gatewayIP().toString() + "\"";
  j += ",\"mac\":\"" + WiFi.macAddress() + "\"";
  j += ",\"hostname\":" + jsonStr(WiFi.getHostname());
  j += ",\"background_ping\":" + String(backgroundPing_ ? "true" : "false");
  uint32_t sinceCheck = (millis() - lastRoamCheck_) / 1000;
  j += ",\"roaming\":{\"enabled\":" + String(roamEnabled_ ? "true" : "false");
  j += ",\"interval_s\":" + String(roamIntervalMs_ / 1000);
  j += ",\"margin_db\":" + String(roamMarginDb_);
  j += ",\"checking\":" + String(roamScanning_ || roamRequested_ ? "true" : "false");
  j += ",\"checks\":" + String(roamChecks_) + ",\"roams\":" + String(roams_);
  j += ",\"next_check_s\":" + String(roamIntervalMs_ / 1000 > sinceCheck ? roamIntervalMs_ / 1000 - sinceCheck : 0);
  j += ",\"last_result\":" + jsonStr(roamResult_);
  j += ",\"last_roam_epoch\":" + String(lastRoamEpoch_);
  j += ",\"last_roam_uptime_s\":" + String((unsigned long)lastRoamUptimeS_) + "}";

  // Statistics of the current telemetry window (without resetting it)
  portENTER_CRITICAL(&pingMux);
  WifiStats w = window_.stats();
  portEXIT_CRITICAL(&pingMux);
  j += ",\"window\":{" + statsJson(w) + "}";

  j += ",\"test\":{\"running\":" + String(testRunning() ? "true" : "false");
  if (testRunning()) {
    j += ",\"id\":" + String(testId_);
    j += ",\"label\":" + jsonStr(meta_.antenna);
    j += ",\"position\":" + jsonStr(meta_.position);
    j += ",\"elapsed_s\":" + String(testElapsedS());
    j += ",\"seconds\":" + String(testSeconds_);
    j += ",\"scanning\":" + String(testScanning() ? "true" : "false");
  }
  j += "}";

  if (includeHistory) {
    static int8_t h[HISTORY];
    int n = history(h, HISTORY);
    j += ",\"history\":[";
    for (int i = 0; i < n; i++) {
      if (i > 0) j += ",";
      j += String((int)h[i]);
    }
    j += "]";
  }
  return j + "}";
}

// One test result. "label" is the antenna name (kept for older clients).
String WifiDiag::resultJson(const WifiTestResult& r) {
  const WifiTestMeta& m = r.meta;
  String j = "{\"id\":" + String(r.id);
  j += ",\"label\":" + jsonStr(m.antenna);
  j += ",\"antenna\":" + jsonStr(m.antenna);
  j += ",\"position\":" + jsonStr(m.position);
  j += ",\"x_ft\":" + jsonNum(m.xFt, 1);
  j += ",\"y_ft\":" + jsonNum(m.yFt, 1);
  j += ",\"height_ft\":" + jsonNum(m.heightFt, 1);
  j += ",\"orientation\":" + jsonStr(m.orientation);
  j += ",\"environment\":" + jsonStr(m.environment);
  j += ",\"note\":" + jsonStr(m.note);
  j += ",\"interval_s\":" + String(r.seconds);
  j += "," + statsJson(r.stats);
  j += ",\"rssi_p10\":" + String(r.rssiP10);
  j += ",\"ping_ms_p95\":" + String(r.pingP95Ms, 1);
  j += ",\"disconnects\":" + String(r.disconnects);
  j += ",\"ap_changes\":" + String(r.apChanges);
  j += ",\"bssid\":\"" + String(r.bssid) + "\"";
  j += ",\"channel\":" + String(r.channel);
  j += ",\"tx_power_dbm\":" + String(r.txPowerDbm, 2);
  j += ",\"scanned\":" + String(r.scanned ? "true" : "false");
  j += ",\"networks_seen\":" + String(r.networksSeen);
  j += ",\"own_aps\":[";
  for (int k = 0; k < r.ownApCount; k++) {
    if (k > 0) j += ",";
    j += "{\"bssid\":\"" + String(r.ownAps[k].bssid) + "\",\"channel\":" + String(r.ownAps[k].channel) +
         ",\"rssi\":" + String(r.ownAps[k].rssi) + "}";
  }
  j += "]";
  j += ",\"same_channel_others\":" + String(r.sameChannelOthers);
  j += ",\"same_channel_strongest\":" + (r.sameChannelOthers ? String(r.sameChannelStrongest) : String("null"));
  j += ",\"epoch\":" + String(r.epoch);
  j += ",\"uptime_s\":" + String((unsigned long)r.uptimeS) + "}";
  return j;
}

String WifiDiag::resultsJson() {
  String j = "{\"running\":" + String(testRunning() ? "true" : "false");
  if (testRunning()) {
    j += ",\"id\":" + String(testId_);
    j += ",\"label\":" + jsonStr(meta_.antenna);
    j += ",\"position\":" + jsonStr(meta_.position);
    j += ",\"elapsed_s\":" + String(testElapsedS());
    j += ",\"seconds\":" + String(testSeconds_);
    j += ",\"scanning\":" + String(testScanning() ? "true" : "false");
  }
  j += ",\"results\":[";
  for (int i = 0; i < resultCount_; i++) {
    if (i > 0) j += ",";
    j += resultJson(results_[i]);
  }
  return j + "]}";
}
