#include "unity_engine.hh"

#include <xdl.h>

#include "utility/config.hh"
#include "utility/oem_policy.hh"
#include "utility/logger.hh"
#include "unity/icall_lookup.hh"

bool Unity::Init(void* handle, bool resolve_resolution) {
    if ((il2cpp_resolve_icall = (il2cpp_resolve_icall_f)xdl_sym(handle, "il2cpp_resolve_icall", nullptr)) == nullptr) {
        ERROR("il2cpp_resolve_icall not found");
        return false;
    }

    set_targetFrameRate = reinterpret_cast<set_targetFrameRate_f>(
        UnityIcall::ResolveFirst(il2cpp_resolve_icall, UnityIcall::kTargetFpsSetters));
    if (!set_targetFrameRate) {
        ERROR("Unity FPS setter icall unavailable: cannot request FPS");
        return false;
    }
    get_targetFrameRate = reinterpret_cast<get_targetFrameRate_f>(
        UnityIcall::ResolveFirst(il2cpp_resolve_icall, UnityIcall::kTargetFpsGetters));

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
