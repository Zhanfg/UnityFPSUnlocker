#ifndef UNITY_STARTUP_PROBE_POLICY_HH
#define UNITY_STARTUP_PROBE_POLICY_HH

#include <cstdint>

namespace UnityStartup {
// Bounded, back-off probing of libil2cpp and its FPS icall.
// The prior 1000ms polling interval routinely lost one-shot startup
// frame-rate setters. Avoid constant CPU work or a perpetual watcher.
constexpr int64_t kProbeDeadlineMs = 30000;
constexpr int kEarlyIntervalMs = 40;
constexpr int kLateIntervalMs = 250;
constexpr int64_t kEarlyPeriodMs = 4000;

inline int SleepMs(int64_t elapsed_ms) {
    if (elapsed_ms >= kProbeDeadlineMs) return 0;
    const int interval = elapsed_ms < kEarlyPeriodMs ? kEarlyIntervalMs : kLateIntervalMs;
    const int64_t remaining = kProbeDeadlineMs - elapsed_ms;
    return remaining < interval ? static_cast<int>(remaining) : interval;
}

inline int StartupDelaySeconds(int configured) {
    // Negative / excessive configuration must not hang initialization.
    if (configured < 0) return 0;
    return configured > 15 ? 15 : configured;
}
} // namespace UnityStartup
#endif
