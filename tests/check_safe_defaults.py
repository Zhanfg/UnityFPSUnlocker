#!/usr/bin/env python3
"""Regression checks for opt-in behavior and OEM rendering preservation."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cfg = json.loads((root / "TargetList.json").read_text(encoding="utf-8"))
global_cfg = cfg["global"]
assert global_cfg["fps"] == 0, "The default must not inject into games"
assert global_cfg["mod_opcode"] is False, "Opcode mutation must be disabled"
assert global_cfg["render_interval"] == 0, "Managed rendering override must be opt-in"
assert global_cfg["scale"] < 0, "Keep OEM resolution untouched by default"
assert cfg["custom"] == {}, "Only user-selected games may be enabled"
main = (root / "UnityFPSUnlocker/main.cc").read_text(encoding="utf-8")
unity = (root / "UnityFPSUnlocker/unity/unity_engine.cc").read_text(encoding="utf-8")
limiter = (root / "UnityFPSUnlocker/fpslimiter.cc").read_text(encoding="utf-8")
assert "OemPolicy::AllowNativeFps(has_custom_cfg_ != 0, current_cfg_.fps_)" in main
assert "access(path.c_str(), F_OK) == 0" not in main, "No automatic IL2CPP game injection"
assert "Utility::NopFunc(reinterpret_cast<unsigned char*>(SetResolution_Internal))" not in unity
assert "EnableFrameRateInterceptor(cfg.fps_)" in limiter, "Require same-thread FPS setter interception"
assert "Unity::GetInstance().SetFrameRate(cfg.fps_, false)" not in limiter, "No off-thread Unity setter calls"
assert "Unity::GetInstance().SetResolution(cfg.scale_)" not in limiter, "No off-thread Unity resolution calls"
assert "DobbyHook(function_address" in unity, "Require game-thread native setter hook"
assert "DobbyDestroy" not in unity, "Do not remove hook while game is running"
print("OEM-preservation static checks passed")
