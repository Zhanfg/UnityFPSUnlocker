#ifndef UNITY_ENGINE_HEADER
#define UNITY_ENGINE_HEADER

#include <stdint.h>

#include "utility/singleton.hh"

struct Resolution {
    int32_t m_Width = 0;
    int32_t m_Height = 0;
    int32_t m_RefreshRate = 0;
};

class ScreenManager {
public:
    virtual ~ScreenManager() {}
    virtual void RequestResolution(int, int, int, void*);
};

class Unity : public Singleton<Unity> {
public:
    // Initialize FPS interface first; expensive resolution probes are opt-in.
    bool Init(void*, bool resolve_resolution = false);

    Resolution GetSystemExtImpl();
    Resolution GetResolution();
    void SetResolution(float);
    // Install a hook that changes *game-originated* setter calls on their original thread.
    // The hook is only armed for an explicitly enabled game.
    bool EnableFrameRateInterceptor(int target_fps);

    // Legacy one-time request path; does NOT guarantee game-thread execution.
    bool SetFrameRate(int, bool);

private:
    using il2cpp_resolve_icall_f = void* (*)(const char*);
    using set_targetFrameRate_f = void (*)(int);
    using get_targetFrameRate_f = int (*)(void);
    using SetResolution_t = void (*)(int, int, int, void*);
    using get_currentResolution_t = void (*)(Resolution*);
    using GetSystemExtImpl_t = void (*)(void*, int*, int*);

    il2cpp_resolve_icall_f il2cpp_resolve_icall = nullptr;
    set_targetFrameRate_f set_targetFrameRate = nullptr;
    get_targetFrameRate_f get_targetFrameRate = nullptr;
    SetResolution_t SetResolution_Internal = nullptr;
    get_currentResolution_t get_currentResolution = nullptr;
    GetSystemExtImpl_t GetSystemExtImpl_Internal = nullptr;

    ScreenManager* screen_manager_ = nullptr;
};

#endif // unity_engine.hh