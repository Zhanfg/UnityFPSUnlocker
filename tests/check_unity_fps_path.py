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
assert "if (!Unity::GetInstance().Init(handle, want_resolution))" in limiter
assert "if (!Unity::GetInstance().SetFrameRate(cfg.fps_, false))" in limiter
assert "bool SetFrameRate(int, bool)" in header
print("Unity FPS-only ABI isolation static checks passed. Live game FPS unverified.")
