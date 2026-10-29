#ifndef FENGCHI_PROBE_HEADER
#define FENGCHI_PROBE_HEADER

// Read-only feature discovery. No device model or SoC assumptions.
// HMBIRD, OPlus game_opt and GEAS may be built independently or hidden by SELinux.
// Presence is a capability hint, NOT proof that Fengchi/144 FPS is enabled.
#include <string>
#include <sys/stat.h>

namespace FengchiProbe {

struct Capabilities {
    bool hmbird_scheduler = false;
    bool game_opt = false;
    bool frame_sync = false;
    bool early_detect = false;
    bool geas_control = false;

    bool HasVendorGamePath() const {
        return game_opt && (hmbird_scheduler || frame_sync || geas_control);
    }
};

inline bool ExistsAt(const std::string& prefix, const char* absolute_path) {
    if (!absolute_path || absolute_path[0] != '/') return false;
    struct stat entry {};
    const std::string full = (prefix.empty() || prefix == "/")
        ? std::string(absolute_path) : prefix + absolute_path;
    return ::stat(full.c_str(), &entry) == 0;
}

inline Capabilities Detect(const std::string& root = "") {
    Capabilities c;
    c.hmbird_scheduler = ExistsAt(root, "/proc/hmbird_sched")
        || ExistsAt(root, "/sys/module/hmbird_sched");
    c.game_opt = ExistsAt(root, "/proc/game_opt")
        || ExistsAt(root, "/sys/module/oplus_game_opt")
        || ExistsAt(root, "/sys/module/oplus_game");
    c.frame_sync = ExistsAt(root, "/proc/game_opt/frame_sync")
        || ExistsAt(root, "/proc/sys/game_frame_sync");
    c.early_detect = ExistsAt(root, "/proc/game_opt/early_detect");
    c.geas_control = ExistsAt(root, "/proc/game_opt/geas_ctrl")
        || ExistsAt(root, "/sys/module/oplus_geas");
    return c;
}

} // namespace FengchiProbe

#endif // FENGCHI_PROBE_HEADER
