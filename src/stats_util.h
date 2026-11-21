// stats_util.h - small statistics helpers (no Arduino dependencies; host-tested)

#pragma once

#include <stddef.h>
#include <stdint.h>

// Nearest-rank percentile (0-100) of n values. The values are sorted in place.
// Returns 0 when n is 0.
//   percentile(rssi, n, 10): the level the signal is at or above 90 % of the time
//   percentile(ping_ms, n, 95): latency that 95 % of the pings beat
template <typename T>
T percentile(T* values, size_t n, int pct) {
  if (n == 0) return 0;
  // Insertion sort: at most 600 samples, run once per test
  for (size_t i = 1; i < n; i++) {
    T v = values[i];
    size_t j = i;
    while (j > 0 && values[j - 1] > v) {
      values[j] = values[j - 1];
      j--;
    }
    values[j] = v;
  }
  if (pct <= 0) return values[0];
  if (pct >= 100) return values[n - 1];
  size_t rank = (size_t)((pct * n + 99) / 100);  // ceil(pct/100 * n), 1-based
  if (rank < 1) rank = 1;
  return values[rank - 1];
}

// Roaming decision: move to another access point of the same network only if it
// is at least marginDb stronger than the current one (hysteresis against
// flip-flopping between access points of similar strength).
inline bool shouldRoam(int currentRssi, int bestOtherRssi, int marginDb) {
  if (bestOtherRssi == 0) return false;  // none heard (0 = no reading)
  return bestOtherRssi >= currentRssi + marginDb;
}
