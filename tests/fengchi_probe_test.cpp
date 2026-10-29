#include "../UnityFPSUnlocker/utility/fengchi_probe.hh"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static void MakeDir(const std::string& path) {
    assert(::mkdir(path.c_str(), 0700) == 0);
}

int main() {
    char pattern[] = "/tmp/fengchi-probe-XXXXXX";
    char* dir = ::mkdtemp(pattern);
    assert(dir != nullptr);
    const std::string root(dir);

    auto caps = FengchiProbe::Detect(root);
    assert(!caps.HasVendorGamePath());
    assert(!caps.hmbird_scheduler);

    MakeDir(root + "/proc");
    MakeDir(root + "/proc/hmbird_sched");
    caps = FengchiProbe::Detect(root);
    assert(caps.hmbird_scheduler);
    assert(!caps.HasVendorGamePath());

    MakeDir(root + "/proc/game_opt");
    MakeDir(root + "/proc/game_opt/frame_sync");
    caps = FengchiProbe::Detect(root);
    assert(caps.game_opt);
    assert(caps.frame_sync);
    assert(caps.HasVendorGamePath());

    assert(::rmdir((root + "/proc/game_opt/frame_sync").c_str()) == 0);
    assert(::rmdir((root + "/proc/game_opt").c_str()) == 0);
    assert(::rmdir((root + "/proc/hmbird_sched").c_str()) == 0);
    assert(::rmdir((root + "/proc").c_str()) == 0);
    assert(::rmdir(root.c_str()) == 0);
    std::puts("Fengchi C++ feature-probe tests passed");
    return 0;
}
