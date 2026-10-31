#include "fpslimiter.hh"

#include <chrono>
#include <thread>

#include "unity/unity_engine.hh"
#include "unity/startup_probe_policy.hh"
#include "utility/oem_policy.hh"
#include "utility/logger.hh"

#include <xdl.h>

namespace FPSLimiter {
    void Start(const ConfigValue& cfg) {
#ifdef __aarch64__
        LOG("[UnityFPSUnlocker][arm64] Starting...");
#elif defined(__ARM_ARCH_7A__)
        LOG("[UnityFPSUnlocker][armv7] Starting...");
#elif defined(__i386__)
        LOG("[UnityFPSUnlocker][x86] Starting...");
#elif defined(__x86_64__)
        LOG("[UnityFPSUnlocker][x86_64] Starting...");
#endif
        cfg.DebugPrint();
        const int delay = UnityStartup::StartupDelaySeconds(cfg.delay_);
        if (delay != cfg.delay_) {
            LOG("Unity FPS startup delay clamped from %d to %d sec", cfg.delay_, delay);
        }
        if (delay > 0) std::this_thread::sleep_for(std::chrono::seconds(delay));

        // Fast bounded probe during initialization, not a 1-second polling
        // loop that may miss the game's one-time native FPS setter call.
        const auto started = std::chrono::steady_clock::now();
        void* handle = nullptr;
        bool ready = false;
        int probe_count = 0;
        while (true) {
            ++probe_count;
            if (!handle) handle = xdl_open("libil2cpp.so", 0);
            if (handle && Unity::GetInstance().Init(handle, false)) {
                ready = true;
                break;
            }
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count();
            const int pause_ms = UnityStartup::SleepMs(elapsed);
            if (pause_ms == 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(pause_ms));
        }

        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        if (!ready) {
            ERROR("Unity FPS hook NOT_ARMED: IL2CPP setter unavailable after %lldms/%d attempts",
                static_cast<long long>(elapsed_ms), probe_count);
            return;
        }
        if (cfg.mod_opcode_) {
            LOG("OEM-safe build: opcode modification was requested but is disabled");
        }
        // Keep Unity resolution handling entirely out of the FPS-only path.
        if (OemPolicy::ShouldSetResolution(cfg.scale_)) {
            LOG("Resolution scaling deferred: no unverified Unity ABI calls");
        }
        if (!Unity::GetInstance().EnableFrameRateInterceptor(cfg.fps_, cfg.render_interval_)) {
            ERROR("Unity native FPS interceptor installation failed");
            return;
        }
        LOG("Unity FPS hook ARMED after %lldms/%d probes, awaiting game-originated setter calls",
            static_cast<long long>(elapsed_ms), probe_count);

        // Read-only counter: no arbitrary off-thread Unity API invocation.
        std::this_thread::sleep_for(std::chrono::seconds(8));
        const unsigned hit_count = Unity::GetInstance().InterceptedFrameRateCalls();
        if (hit_count == 0) {
            ERROR("Unity FPS hook NO_HIT after 8s: game did not call the intercepted setter; FPS unlock unverified");
        } else {
            LOG("Unity FPS hook HIT count=%u; native request intercepted, actual presented FPS still unverified",
                hit_count);
        }
    }
} // namespace FPSLimiter
