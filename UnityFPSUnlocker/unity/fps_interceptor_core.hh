#ifndef UNITY_FPS_INTERCEPTOR_CORE_HH
#define UNITY_FPS_INTERCEPTOR_CORE_HH

// Pure native FPS policy. This is independently unit-testable without Android
// or the hook backend, so resolution and OEM GPU controls cannot creep in.
namespace UnityFramePolicy {
constexpr int kMinFps = 30;
constexpr int kMaxFps = 240;

inline bool ValidTarget(int target) {
    return target >= kMinFps && target <= kMaxFps;
}

inline int Apply(int game_requested, int user_target) {
    return ValidTarget(user_target) ? user_target : game_requested;
}
} // namespace UnityFramePolicy
#endif
