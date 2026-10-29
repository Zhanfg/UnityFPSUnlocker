#include "../UnityFPSUnlocker/utility/oem_policy.hh"
#include <cassert>
#include <limits>

int main() {
    using OemPolicy::AllowNativeFps;
    using OemPolicy::ShouldSetResolution;
    assert(!AllowNativeFps(false, 120));
    assert(!AllowNativeFps(true, 0));
    assert(!AllowNativeFps(true, 29));
    assert(!AllowNativeFps(true, 241));
    assert(AllowNativeFps(true, 30));
    assert(AllowNativeFps(true, 60));
    assert(AllowNativeFps(true, 120));
    assert(AllowNativeFps(true, 144));
    assert(AllowNativeFps(true, 165));
    assert(AllowNativeFps(true, 185));
    assert(AllowNativeFps(true, 240));
    assert(!ShouldSetResolution(-1.0f));
    assert(!ShouldSetResolution(0.0f));
    assert(!ShouldSetResolution(1.0f));
    assert(!ShouldSetResolution(0.1f));
    assert(!ShouldSetResolution(3.0f));
    assert(!ShouldSetResolution(std::numeric_limits<float>::infinity()));
    assert(!ShouldSetResolution(std::numeric_limits<float>::quiet_NaN()));
    assert(ShouldSetResolution(0.75f));
    assert(ShouldSetResolution(1.25f));
    return 0;
}
