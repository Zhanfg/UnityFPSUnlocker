#ifndef OEM_POLICY_HEADER
#define OEM_POLICY_HEADER

#include <cmath>

namespace OemPolicy {

// Never touch a game unless the user explicitly listed its process.
// A reasonable bound also prevents accidental extreme FPS values.
inline bool AllowNativeFps(bool explicitly_selected, int fps) {
    return explicitly_selected && fps >= 30 && fps <= 240;
}

// Keep the system's resolution and vendor super-resolution intact by default.
// Scaling is an explicit per-game experiment, never a global default.
inline bool ShouldSetResolution(float scale) {
    return std::isfinite(scale) && scale >= 0.25f && scale <= 2.0f
        && std::fabs(scale - 1.0f) > 0.0001f;
}

} // namespace OemPolicy

#endif // OEM_POLICY_HEADER
