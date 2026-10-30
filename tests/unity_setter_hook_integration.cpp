#include "../UnityFPSUnlocker/unity/fps_interceptor_core.hh"
#include <dobby.h>
#include <cassert>
#include <cstdio>

static volatile int last_set = -999;
static int override_fps = 144;
static void (*original_setter)(int) = nullptr;

extern "C" __attribute__((noinline)) void MockUnitySetTargetFrameRate(int fps) {
    last_set = fps;
}
static void HookUnitySetTargetFrameRate(int game_fps) {
    assert(original_setter != nullptr);
    original_setter(UnityFramePolicy::Apply(game_fps, override_fps));
}

int main() {
    MockUnitySetTargetFrameRate(30);
    assert(last_set == 30);
    void* trampoline = nullptr;
    const int installed = DobbyHook(reinterpret_cast<void*>(MockUnitySetTargetFrameRate),
        reinterpret_cast<void*>(HookUnitySetTargetFrameRate), &trampoline);
    if (installed != 0 || !trampoline) {
        std::fprintf(stderr, "DobbyHook failed rc=%d trampoline=%p\n", installed, trampoline);
        return 1;
    }
    original_setter = reinterpret_cast<void (*)(int)>(trampoline);
    MockUnitySetTargetFrameRate(30);
    assert(last_set == 144);
    MockUnitySetTargetFrameRate(60);
    assert(last_set == 144);
    override_fps = 165;
    MockUnitySetTargetFrameRate(90);
    assert(last_set == 165);
    override_fps = 0;
    MockUnitySetTargetFrameRate(30);
    assert(last_set == 30);
    if (DobbyDestroy(reinterpret_cast<void*>(MockUnitySetTargetFrameRate)) != 0) {
        std::fputs("DobbyDestroy failed\n", stderr);
        return 2;
    }
    MockUnitySetTargetFrameRate(60);
    assert(last_set == 60);
    std::puts("Unity native setter hook integration verified: replace, repeat, trampoline, restore");
    return 0;
}
