// Host test stand-in for ESPmDNS: records the host name and services.
#pragma once
#include <string>
#include <vector>

class FakeMDNS {
 public:
  bool ok = true;
  std::string hostname;
  std::vector<std::string> services;  // "name/proto/port"
  bool begin(const char* h) { hostname = h; return ok; }
  void addService(const char* name, const char* proto, unsigned short port) {
    services.push_back(std::string(name) + "/" + proto + "/" + std::to_string(port));
  }
};
extern FakeMDNS MDNS;
