#ifndef UNITY_ICALL_LOOKUP_HEADER
#define UNITY_ICALL_LOOKUP_HEADER

// Runtime-only lookup. Unity's icall strings vary across engine versions.
// No code patching, IL2CPP metadata mutation or OEM framework changes.
namespace UnityIcall {
using Resolver = void* (*)(const char*);

inline void* ResolveFirst(Resolver resolver, const char* const* names) {
    if (!resolver || !names) return nullptr;
    for (const char* const* it = names; *it; ++it) {
        if (void* target = resolver(*it)) return target;
    }
    return nullptr;
}

static constexpr const char* kTargetFpsSetters[] = {
    "UnityEngine.Application::set_targetFrameRate",
    "UnityEngine.Application::set_targetFrameRate(System.Int32)",
    nullptr
};
static constexpr const char* kTargetFpsGetters[] = {
    "UnityEngine.Application::get_targetFrameRate",
    "UnityEngine.Application::get_targetFrameRate()",
    nullptr
};
} // namespace UnityIcall

#endif // UNITY_ICALL_LOOKUP_HEADER
