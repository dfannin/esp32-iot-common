// Host test stand-in for the ESP32 WebServer: routes are called by request().
#pragma once
#include <Arduino.h>

#include <functional>
#include <map>
#include <string>

enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_POST };
enum HTTPAuthMethod { BASIC_AUTH, DIGEST_AUTH };

class WebServer {
 public:
  using Handler = std::function<void()>;
  std::map<std::string, Handler> routes;  // "GET /path", "POST /path"
  std::map<std::string, std::string> args;
  std::string authUser, authPass;  // what the browser sends
  int collected = -1;
  bool begun = false;
  // Last response
  int code = 0;
  std::string type, body, realm;
  std::map<std::string, std::string> headers;

  explicit WebServer(int = 80) {}
  void on(const char* path, HTTPMethod m, Handler h) { routes[(m == HTTP_POST ? "POST " : "GET ") + std::string(path)] = h; }
  bool request(const char* method, const char* path) {
    code = 0; type.clear(); body.clear(); headers.clear(); realm.clear();
    auto it = routes.find(std::string(method) + " " + path);
    if (it == routes.end()) return false;
    it->second();
    return true;
  }
  bool hasArg(const char* n) { return args.count(n) > 0; }
  String arg(const char* n) {
    auto it = args.find(n);
    return it == args.end() ? String("") : String(it->second.c_str());
  }
  bool authenticate(const char* u, const char* p) { return authUser == u && authPass == p; }
  void requestAuthentication(HTTPAuthMethod, const char* r, const char*) { code = 401; realm = r; }
  void sendHeader(const char* k, const char* v) { headers[k] = v; }
  void send(int c, const char* t, const String& b) { code = c; type = t; body = b.c_str(); }
  void send_P(int c, const char* t, const char* d, size_t len) { code = c; type = t; body.assign(d, len); }
  void collectHeaders(const char**, size_t n) { collected = (int)n; }
  void begin() { begun = true; }
};
