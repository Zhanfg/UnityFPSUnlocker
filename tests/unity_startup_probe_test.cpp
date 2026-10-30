#include "../UnityFPSUnlocker/unity/startup_probe_policy.hh"
#include <cassert>
#include <cstdint>
int main() {
    using UnityStartup::SleepMs;
    using UnityStartup::StartupDelaySeconds;
    assert(SleepMs(0) == 40);
    assert(SleepMs(3999) == 40);
    assert(SleepMs(4000) == 250);
    assert(SleepMs(29999) == 1);
    assert(SleepMs(30000) == 0);
    assert(SleepMs(35000) == 0);
    assert(StartupDelaySeconds(-1) == 0);
    assert(StartupDelaySeconds(0) == 0);
    assert(StartupDelaySeconds(8) == 8);
    assert(StartupDelaySeconds(60) == 15);
    int64_t elapsed = 0;
    unsigned iterations = 0;
    while (int delay = SleepMs(elapsed)) {
        elapsed += delay;
        ++iterations;
        assert(iterations < 1000);
    }
    assert(elapsed == 30000);
    assert(iterations > 150 && iterations < 250);
    return 0;
}
