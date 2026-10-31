#include "../UnityFPSUnlocker/unity/fps_interceptor_core.hh"
#include <cassert>
int main() {
    using UnityFramePolicy::Apply;
    assert(Apply(30, 144) == 144);
    assert(Apply(60, 144) == 144);
    assert(Apply(90, 144) == 144);
    assert(Apply(-1, 144) == 144);
    assert(Apply(30, 120) == 120);
    assert(Apply(60, 165) == 165);
    assert(Apply(30, 240) == 240);
    assert(Apply(30, 0) == 30);
    assert(Apply(-1, 0) == -1);
    assert(Apply(90, 241) == 90);
    assert(Apply(90, 29) == 90);
    return 0;
}
