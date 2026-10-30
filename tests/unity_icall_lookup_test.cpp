#include "../UnityFPSUnlocker/unity/icall_lookup.hh"
#include <cassert>
#include <cstring>

namespace {
int calls=0;
void* sentinel=static_cast<void*>(&calls);
void* FirstNameOnly(const char* s) {
    ++calls;
    if (std::strcmp(s,"UnityEngine.Application::set_targetFrameRate")==0) return sentinel;
    return nullptr;
}
void* SecondNameOnly(const char* s) {
    ++calls;
    if (std::strcmp(s,"UnityEngine.Application::set_targetFrameRate(System.Int32)")==0) return sentinel;
    return nullptr;
}
void* None(const char*) { ++calls; return nullptr; }
}
int main() {
    using UnityIcall::ResolveFirst;
    calls=0;
    assert(ResolveFirst(FirstNameOnly,UnityIcall::kTargetFpsSetters)==sentinel);
    assert(calls==1);
    calls=0;
    assert(ResolveFirst(SecondNameOnly,UnityIcall::kTargetFpsSetters)==sentinel);
    assert(calls==2);
    calls=0;
    assert(ResolveFirst(None,UnityIcall::kTargetFpsSetters)==nullptr);
    assert(calls==2);
    calls=0;
    assert(ResolveFirst(nullptr,UnityIcall::kTargetFpsGetters)==nullptr);
    assert(calls==0);
    return 0;
}
