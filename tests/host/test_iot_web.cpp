// Host tests for iot_web.h and web_args.h (with stand-ins in fakes/): make -C tests/host

#include <cstdio>
#include <cstring>

#include <WebServer.h>
#include <WiFi.h>

#include "../../src/iot_web.h"

uint32_t fake_millis = 0;
int64_t fake_uptime_us = 0;
uint32_t fake_cpu_mhz = 80;
FakeSerial Serial;
FakeWiFi WiFi;
FakeEsp ESP;
WifiDiag wifiDiag;
float WifiDiag::tx = 19.5;
bool WifiDiag::sleep = false;
void configTzTime(const char*, const char*, const char*, const char*) {}
bool getLocalTime(struct tm*, uint32_t) { return false; }

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                               \
  do {                                                            \
    checks++;                                                     \
    if (!(cond)) {                                                \
      failures++;                                                 \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                             \
  } while (0)

static bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }
static bool gzipped(const WebServer& s) {
  return s.headers.count("Content-Encoding") && s.headers.at("Content-Encoding") == "gzip" && s.body.size() > 2 &&
         (uint8_t)s.body[0] == 0x1f && (uint8_t)s.body[1] == 0x8b;
}
static String statusJson() { return "{\"device\":{\"name\":\"test00\"}}"; }
static const uint8_t DEVICE_JS[] = {0x1f, 0x8b, 0x08, 0x00};

static void test_args() {
  long l = 0;
  float f = 0;
  CHECK(webParseLong("115200", 1200, 115200, &l) && l == 115200);
  CHECK(webParseLong(" 60 ", 10, 600, &l) && l == 60);
  CHECK(!webParseLong("5", 10, 600, &l));
  CHECK(!webParseLong("60s", 10, 600, &l));
  CHECK(!webParseLong("", 10, 600, &l));
  CHECK(!webParseLong(nullptr, 10, 600, &l));
  CHECK(!webParseLong("99999999999999999999", 0, 10, &l));
  CHECK(webParseFloat("-12.5", -100, 100, &f) && f == -12.5f);
  CHECK(!webParseFloat("abc", -100, 100, &f));
  CHECK(!webParseFloat("nan", -100, 100, &f));
  CHECK(!webParseFloat("19.6", 2, 19.5, &f));
  CHECK(webParseBool("1") && webParseBool("true") && webParseBool("on"));
  CHECK(!webParseBool("0") && !webParseBool("yes") && !webParseBool(nullptr) && !webParseBool("onx"));
}

static void test_pages() {
  WebServer server(80);
  IotWeb web;
  IotWebConfig c;
  c.user = "admin";
  c.password = "pw";
  c.realm = "test00";
  c.status = statusJson;
  web.begin(server, c);
  CHECK(server.begun && server.collected == 0);

  for (const char* p : {"/", "/wifi"}) {
    CHECK(server.request("GET", p));
    CHECK(server.code == 200 && server.type == "text/html" && gzipped(server));
    CHECK(server.body.size() == IOT_WEB_PAGE_HTML_LEN);
  }
  CHECK(server.request("GET", "/style.css") && server.type == "text/css" && gzipped(server));
  CHECK(server.request("GET", "/app.js") && server.type == "application/javascript" && gzipped(server));
  CHECK(server.headers["Cache-Control"] == "no-cache");
  CHECK(server.request("GET", "/device.js") && server.code == 200 && !gzipped(server));  // none: empty script
  CHECK(server.request("GET", "/status") && server.body == "{\"device\":{\"name\":\"test00\"}}");
  CHECK(server.headers["Cache-Control"] == "no-store");

  CHECK(!server.request("GET", "/sim"));
  web.addPage("/sim");
  CHECK(server.request("GET", "/sim") && gzipped(server) && server.body.size() == IOT_WEB_PAGE_HTML_LEN);

  WebServer s2;
  IotWeb w2;
  c.deviceJs = DEVICE_JS;
  c.deviceJsLen = sizeof(DEVICE_JS);
  w2.begin(s2, c);
  CHECK(s2.request("GET", "/device.js") && gzipped(s2) && s2.body.size() == sizeof(DEVICE_JS));
}

static void test_auth_and_results() {
  WebServer server;
  IotWeb web;
  IotWebConfig c;
  c.user = "admin";
  c.password = "pw";
  c.realm = "test00";
  web.begin(server, c);

  server.args = {{"tx_power", "15"}};
  CHECK(server.request("POST", "/api/wifi/config") && server.code == 401 && server.realm == "test00");
  server.authUser = "admin";
  server.authPass = "pw";
  CHECK(server.request("POST", "/api/wifi/config") && server.code == 200);
  CHECK(has(server.body, "\"ok\":true") && has(server.body, "\"result\":\"DONE\"") && has(server.body, "TX power 15.00 dBm"));
  server.args = {{"tx_power", "25"}};
  CHECK(server.request("POST", "/api/wifi/config") && server.code == 400 && has(server.body, "tx_power must be 2 to 19.5"));
  server.args = {{"sleep", "1"}};
  CHECK(server.request("POST", "/api/wifi/config") && WifiDiag::sleep && has(server.body, "Modem sleep on."));
  server.args = {};
  CHECK(server.request("POST", "/api/wifi/config") && has(server.body, "Nothing changed"));

  server.args = {{"seconds", "5"}};
  CHECK(server.request("POST", "/api/wifi/test") && server.code == 400);
  server.args = {{"seconds", "60"}, {"x_ft", "abc"}};
  CHECK(server.request("POST", "/api/wifi/test") && server.code == 400 && has(server.body, "x_ft must be a number"));
  server.args = {{"seconds", "120"}, {"antenna", "whip \"A\""}, {"x_ft", " -3.5 "}, {"position", "kitchen"}};
  CHECK(server.request("POST", "/api/wifi/test") && server.code == 200 && has(server.body, "\"id\":1"));
  CHECK(has(server.body, "Test started: whip \\\"A\\\""));  // escaped
  CHECK(wifiDiag.lastSeconds == 120 && wifiDiag.lastMeta.xFt == -3.5f && std::strcmp(wifiDiag.lastMeta.position, "kitchen") == 0);
  CHECK(server.request("POST", "/api/wifi/test") && server.code == 409);
  CHECK(server.request("POST", "/api/wifi/roam") && server.code == 409);
  CHECK(server.request("GET", "/api/wifi/scan") && server.code == 200);
  server.args = {{"start", "1"}};
  CHECK(server.request("GET", "/api/wifi/scan") && server.code == 409);
  wifiDiag.busy = false;
  CHECK(server.request("POST", "/api/wifi/roam") && server.code == 200 && has(server.body, "ACCEPTED"));
  CHECK(server.request("GET", "/api/wifi") && server.body == "{}");
  server.args = {{"history", "1"}};
  CHECK(server.request("GET", "/api/wifi") && server.body == "{\"history\":[]}");
}

int main() {
  test_args();
  test_pages();
  test_auth_and_results();
  std::printf("iot_web: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
