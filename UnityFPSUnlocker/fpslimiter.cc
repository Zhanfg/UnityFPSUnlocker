#include "fpslimiter.hh"

#include <chrono>
#include <string>
#include <thread>

#include "unity/unity_engine.hh"
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
        std::chrono::seconds sleep_duration(cfg.delay_);
        std::this_thread::sleep_for(sleep_duration);

        LOG("***** begin *****");
        void* handle = nullptr;
        for (int i = 0; i < 30; ++i) {
            if ((handle = xdl_open("libil2cpp.so", 0))) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        if (handle) {
            const bool want_resolution = OemPolicy::ShouldSetResolution(cfg.scale_);
            // libil2cpp may be mapped before icall tables become available.
            // Retry symbol resolution after Unity initialization, without writing
            // executable pages or modifying the OEM scheduling stack.
            bool ready = false;
            for (int attempt = 0; attempt < 12; ++attempt) {
                if (Unity::GetInstance().Init(handle, false)) {
                    ready = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            if (!ready) {
                ERROR("Unity FPS initialization failed after icall retries; no FPS request performed");
                return;
            }
            if (cfg.mod_opcode_) {
                LOG("OEM-safe build: opcode modification was requested but is disabled");
            }
            if (!Unity::GetInstance().EnableFrameRateInterceptor(cfg.fps_, cfg.render_interval_)) {
                ERROR("Unity native FPS interceptor installation failed");
                return;
            }
            // Do not call Application.targetFrameRate from this worker.
            // The game must call the setter to trigger our same-thread hook.
            // Resolution modification is deliberately not performed here;
            // it requires a separately validated engine-thread ABI pathway.
            if (want_resolution) {
                LOG("Resolution scale was requested but deferred to protect Unity thread and OEM rendering");
            }
            LOG("Unity FPS interceptor armed; awaiting game setter calls (not proof of effective FPS)");
            LOG("***** finished *****");
            return;
        }
        else {
            ERROR("Failed to open libil2cpp.so");
        }
    }
} // namespace FPSLimiter
