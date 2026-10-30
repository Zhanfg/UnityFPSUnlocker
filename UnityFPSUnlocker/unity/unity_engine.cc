#include "unity_engine.hh"

#include <xdl.h>

#include "utility/config.hh"
#include "utility/oem_policy.hh"
#include "utility/logger.hh"
#include "unity/icall_lookup.hh"
#include "unity/fps_interceptor_core.hh"
#include "unity/ondemand_runtime.hh"

#include <atomic>

#if defined(__aarch64__)
#include <dobby.h>
#endif

namespace {
UnityOnDemand::Runtime g_ondemand_runtime;
// Early in process startup, Unity may expose libil2cpp.so before its icall
// registry is ready. Fast probing must not flood Android logcat.
std::atomic<unsigned> g_init_unready_count{0};
bool ShouldReportUnready() {
    const unsigned n = g_init_unready_count.fetch_add(1, std::memory_order_relaxed) + 1;
    return n <= 3 || n % 64 == 0;
}
} // namespace

bool Unity::Init(void* handle, bool resolve_resolution) {
    if ((il2cpp_resolve_icall = (il2cpp_resolve_icall_f)xdl_sym(handle, "il2cpp_resolve_icall", nullptr)) == nullptr) {
        if (ShouldReportUnready()) ERROR("il2cpp_resolve_icall not found yet");
        return false;
    }

    set_targetFrameRate = reinterpret_cast<set_targetFrameRate_f>(
        UnityIcall::ResolveFirst(il2cpp_resolve_icall, UnityIcall::kTargetFpsSetters));
    if (!set_targetFrameRate) {
        if (ShouldReportUnready()) ERROR("Unity FPS setter icall not ready yet");
        return false;
    }
    get_targetFrameRate = reinterpret_cast<get_targetFrameRate_f>(
        UnityIcall::ResolveFirst(il2cpp_resolve_icall, UnityIcall::kTargetFpsGetters));

    // Resolve functions only: actual IL2CPP runtime metadata and field reads
    // happen later, on the original game's setter invocation thread.
    UnityOnDemand::Api ondemand_api{};
    ondemand_api.domain_get = reinterpret_cast<decltype(ondemand_api.domain_get)>(
        xdl_sym(handle, "il2cpp_domain_get", nullptr));
    ondemand_api.domain_assembly_open = reinterpret_cast<decltype(ondemand_api.domain_assembly_open)>(
        xdl_sym(handle, "il2cpp_domain_assembly_open", nullptr));
    ondemand_api.assembly_get_image = reinterpret_cast<decltype(ondemand_api.assembly_get_image)>(
        xdl_sym(handle, "il2cpp_assembly_get_image", nullptr));
    ondemand_api.class_from_name = reinterpret_cast<decltype(ondemand_api.class_from_name)>(
        xdl_sym(handle, "il2cpp_class_from_name", nullptr));
    ondemand_api.class_get_field_from_name = reinterpret_cast<decltype(ondemand_api.class_get_field_from_name)>(
        xdl_sym(handle, "il2cpp_class_get_field_from_name", nullptr));
    ondemand_api.field_get_type = reinterpret_cast<decltype(ondemand_api.field_get_type)>(
        xdl_sym(handle, "il2cpp_field_get_type", nullptr));
    ondemand_api.type_get_type = reinterpret_cast<decltype(ondemand_api.type_get_type)>(
        xdl_sym(handle, "il2cpp_type_get_type", nullptr));
    ondemand_api.field_get_flags = reinterpret_cast<decltype(ondemand_api.field_get_flags)>(
        xdl_sym(handle, "il2cpp_field_get_flags", nullptr));
    ondemand_api.field_static_get_value = reinterpret_cast<decltype(ondemand_api.field_static_get_value)>(
        xdl_sym(handle, "il2cpp_field_static_get_value", nullptr));
    ondemand_api.field_static_set_value = reinterpret_cast<decltype(ondemand_api.field_static_set_value)>(
        xdl_sym(handle, "il2cpp_field_static_set_value", nullptr));
    g_ondemand_runtime.Init(ondemand_api);

    // FPS-only operation never probes/executes Unity resolution internal calls.
    // Resolution ABI changed across engine versions; keep this opt-in.
    if (!resolve_resolution) return true;

    if ((get_currentResolution = (get_currentResolution_t)il2cpp_resolve_icall("UnityEngine.Screen::get_currentResolution_Injected")) == nullptr) {
        ERROR("get_currentResolution not found");
    }

    if ((SetResolution_Internal = (SetResolution_t)il2cpp_resolve_icall("UnityEngine.Screen::SetResolution")) == nullptr) {
        if ((SetResolution_Internal = (SetResolution_t)il2cpp_resolve_icall("UnityEngine.Screen::SetResolution_Injected")) == nullptr) {
            ERROR("SetResolution not found");

            if (auto get_height_ptr = il2cpp_resolve_icall("UnityEngine.Screen::get_height")) {
                auto ptr = reinterpret_cast<uint32_t*>(get_height_ptr);
                auto opcode = ptr[1];
                if ((opcode & 0xFC000000) == 0x94000000) {
                    auto offset = (int32_t)(opcode << 8) >> 6;
                    auto addr = reinterpret_cast<intptr_t>(get_height_ptr) + 4 + offset;
                    auto GetScreenManager = reinterpret_cast<void* (*)(void)>(addr);
                    screen_manager_ = reinterpret_cast<ScreenManager*>(GetScreenManager());
                }
            }

            if (screen_manager_ == nullptr) {
                ERROR("ScreenManager not found");
            }
        }
    }

    if ((GetSystemExtImpl_Internal = (GetSystemExtImpl_t)il2cpp_resolve_icall("UnityEngine.Display::GetSystemExtImpl")) == nullptr) {
        ERROR("GetSystemExtImpl not found");
    }
    return true;
}

Resolution Unity::GetSystemExtImpl() {
    Resolution resolution;
    if (GetSystemExtImpl_Internal) {
        GetSystemExtImpl_Internal(nullptr, &resolution.m_Width, &resolution.m_Height);
    }
    return resolution;
}

Resolution Unity::GetResolution() {
    Resolution resolution;
    if (get_currentResolution) {
        get_currentResolution(&resolution);
    }
    return resolution;
}

void Unity::SetResolution(float scale) {
    if (!OemPolicy::ShouldSetResolution(scale)) {
        return;
    }
    Resolution resolution = GetSystemExtImpl();
    if (resolution.m_Width > 0 && resolution.m_Height > 0) {
        auto target_width = static_cast<int32_t>(resolution.m_Width * scale);
        auto target_height = static_cast<int32_t>(resolution.m_Height * scale);
        int preferred_refresh_rate = 0;

        if (SetResolution_Internal) {
            LOG("Set resolution: %d x %d", target_width, target_height);
            SetResolution_Internal(target_width, target_height, 1, &preferred_refresh_rate);
            // Preserve Unity and OEM rendering pipelines: never replace this
            // function with an unconditional return instruction.
        }
        else if (screen_manager_) {
            LOG("Set resolution: %d x %d", target_width, target_height);
            screen_manager_->RequestResolution(target_width, target_height, 1, &preferred_refresh_rate);
        }
    }
}

bool Unity::SetFrameRate(int framerate, bool mod_opcode) {
    if (!set_targetFrameRate || framerate < 30 || framerate > 240) {
        ERROR("Unity FPS request rejected: setter unavailable or invalid FPS %d", framerate);
        return false;
    }
    if (mod_opcode) {
        LOG("Opcode patching is disabled to preserve game and OEM rendering");
    }
    // This call issues a Unity engine request. It does not prove the actual
    // rendered FPS, screen Hz, nor OEM-generated frame interpolation.
    set_targetFrameRate(framerate);
    LOG("Unity native FPS request submitted: %d", framerate);
    if (get_targetFrameRate) {
        const int reported = get_targetFrameRate();
        LOG("Unity native FPS getter reads back %d after request %d", reported, framerate);
        if (reported != framerate) {
            ERROR("Unity FPS request not retained at readback: requested=%d reported=%d", framerate, reported);
        }
    } else {
        LOG("Unity FPS getter unavailable; only setter call can be verified");
    }
    return true;
}

#if defined(__aarch64__)
namespace {
using TargetSetter = void (*)(int);
static std::atomic<int> g_requested_fps{0};
static std::atomic<unsigned> g_intercepted_calls{0};
static std::atomic<bool> g_force_ondemand{false};
static TargetSetter g_original_setter = nullptr;
static int (*g_target_fps_getter)() = nullptr;
static void* g_hook_target = nullptr;

// Called by the game itself, on the thread that originally invoked the
// Unity Application.targetFrameRate native setter. No synthetic Unity API
// calls from a Zygisk worker thread, busy loop, polling or timer.
static void InterceptTargetFrameRate(int game_fps) {
    const TargetSetter original = g_original_setter;
    if (!original) {
        // The tiny hook-installation window can race with an app setter call.
        // Avoid recursion or calling an uninitialized trampoline.
        ERROR("Unity setter hook trampoline not ready; skipped one setter");
        return;
    }
    const int desired = g_requested_fps.load(std::memory_order_relaxed);
    const int actual_argument = UnityFramePolicy::Apply(game_fps, desired);
    original(actual_argument);
    // OnDemandRendering is a managed static field (not an IL2CPP icall).
    // Inspect it on this original game thread. Only opt-in per-game config
    // may change the interval, and never before calling the original setter.
    const bool force_interval = g_force_ondemand.load(std::memory_order_relaxed);
    const auto interval = g_ondemand_runtime.Observe(force_interval);
    const unsigned call_index =
        g_intercepted_calls.fetch_add(1, std::memory_order_relaxed);
    if (call_index < 12 || interval.changed) {
        LOG("Unity OnDemandRendering available=%d interval=%d->%d force=%d",
            interval.available, interval.before, interval.after, force_interval);
    }
    if (call_index < 12) {
        if (g_target_fps_getter) {
            const int readback = g_target_fps_getter();
            LOG("Unity setter interception call=%u game=%d override=%d forwarded=%d readback=%d",
                call_index + 1, game_fps, desired, actual_argument, readback);
        } else {
            LOG("Unity setter interception call=%u game=%d override=%d forwarded=%d readback=unavailable",
                call_index + 1, game_fps, desired, actual_argument);
        }
    }
}
} // namespace
#endif

unsigned Unity::InterceptedFrameRateCalls() const {
#if defined(__aarch64__)
    return g_intercepted_calls.load(std::memory_order_relaxed);
#else
    return 0;
#endif
}

bool Unity::EnableFrameRateInterceptor(int target_fps, bool force_render_interval) {
    if (!UnityFramePolicy::ValidTarget(target_fps) || !set_targetFrameRate) {
        ERROR("Unity setter hook rejected: missing setter or target=%d", target_fps);
        return false;
    }
#if defined(__aarch64__)
    void* function_address = reinterpret_cast<void*>(set_targetFrameRate);
    if (g_hook_target && g_hook_target != function_address) {
        ERROR("Unity setter hook already installed for a different address");
        return false;
    }
    if (!g_hook_target) {
        g_requested_fps.store(target_fps, std::memory_order_relaxed);
        g_force_ondemand.store(force_render_interval, std::memory_order_relaxed);
        g_target_fps_getter = get_targetFrameRate;
        // DobbyHook() performs Prepare+Commit in one call, opening a race:
        // a game setter can hit the replacement before origin_call is assigned.
        // Use the upstream two-phase API to publish the trampoline FIRST.
        void* trampoline = nullptr;
        const int prepared = DobbyPrepare(function_address,
            reinterpret_cast<void*>(InterceptTargetFrameRate), &trampoline);
        if (prepared != 0 || trampoline == nullptr) {
            ERROR("DobbyPrepare Unity setter failed: rc=%d trampoline=%p", prepared, trampoline);
            g_requested_fps.store(0, std::memory_order_relaxed);
            return false;
        }
        g_original_setter = reinterpret_cast<TargetSetter>(trampoline);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        const int committed = DobbyCommit(function_address);
        if (committed != 0) {
            ERROR("DobbyCommit Unity setter failed: rc=%d; attempting teardown", committed);
            // Unlike failure to prepare, this may have partially patched the
            // function. Attempt to restore it before resetting our trampoline.
            const int destroyed = DobbyDestroy(function_address);
            if (destroyed != 0) {
                ERROR("DobbyDestroy failed after commit error: rc=%d; preserving trampoline", destroyed);
                return false;
            }
            g_original_setter = nullptr;
            g_requested_fps.store(0, std::memory_order_release);
            return false;
        }
        g_hook_target = function_address;
        LOG("Unity setter hook armed: target=%d (awaiting actual game setter calls)", target_fps);
    } else {
        g_requested_fps.store(target_fps, std::memory_order_release);
        g_force_ondemand.store(force_render_interval, std::memory_order_release);
        LOG("Unity setter hook updated target=%d force_ondemand=%d", target_fps, force_render_interval);
    }
    return true;
#else
    ERROR("Unity setter hook only implemented and compiled for ARM64");
    return false;
#endif
}
