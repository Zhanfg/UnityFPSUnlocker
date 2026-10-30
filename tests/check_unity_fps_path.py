#!/usr/bin/env python3
"""Source-level regression guards. Does NOT prove actual game FPS."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
engine = (root / "UnityFPSUnlocker/unity/unity_engine.cc").read_text()
limiter = (root / "UnityFPSUnlocker/fpslimiter.cc").read_text()
header = (root / "UnityFPSUnlocker/unity/unity_engine.hh").read_text()

assert "bool Unity::Init(void* handle, bool resolve_resolution)" in engine
assert "if (!resolve_resolution) return true;" in engine
assert 'UnityIcall::kTargetFpsSetters' in engine
assert 'UnityIcall::kTargetFpsGetters' in engine
fps_body = engine.split("bool Unity::SetFrameRate(int framerate, bool mod_opcode)", 1)[1]
assert "set_targetFrameRate(framerate)" in fps_body
assert "GetResolution" not in fps_body, "FPS setter may not call old Unity Resolution ABI"
assert "Utility::NopFunc" not in fps_body, "Do not patch executable instructions"
assert "GetResolution();" not in fps_body
assert "const bool want_resolution = OemPolicy::ShouldSetResolution(cfg.scale_);" in limiter
assert "Unity::GetInstance().Init(handle, want_resolution)" in limiter
assert "for (int attempt = 0; attempt < 12; ++attempt)" in limiter
assert "if (!Unity::GetInstance().EnableFrameRateInterceptor(cfg.fps_))" in limiter
assert "Unity::GetInstance().SetFrameRate(cfg.fps_, false)" not in limiter
assert "bool SetFrameRate(int, bool)" in header
print("Unity FPS-only ABI isolation static checks passed. Live game FPS unverified.")

assert "original(actual_argument);" in engine, "Forward to native Unity setter in the original caller"
assert "UnityFramePolicy::Apply(game_fps, desired)" in engine, "Intercept all game FPS overwrites"
assert "g_intercepted_calls.fetch_add" in engine, "Expose observed interception evidence"
assert "SetResolution(cfg.scale_)" not in limiter, "Do not call Unity resolution from worker"
