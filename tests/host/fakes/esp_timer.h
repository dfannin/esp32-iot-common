#pragma once
#include <cstdint>
extern int64_t fake_uptime_us;
inline int64_t esp_timer_get_time() { return fake_uptime_us; }
