# Unified FPS + Fengchi: release gates

**Not ready for user module delivery.** Keep current code as a development branch, even when ARM64 CI passes. The native Fengchi Direct Lab ZIP is a kernel-interface research tool, not a universal FPS unlocker.

## Known blockers / 尚未完成
- [ ] Unity FPS setter invoked **from a verified Unity-supported execution context**, not merely a Zygisk-created background thread. Unity does not guarantee use of its API from arbitrary native threads.
- [ ] Confirm running-game Unity IL2CPP frame-rate requests persist when game code changes FPS after startup. Do not rely on one-time setter call or unsafe opcode NOP.
- [ ] Verify requested target against native engine/actual frame presentation and display refresh separately, with at least two Unity versions and reproducible diagnostics.
- [ ] Implement and test UE4/UE5 Frame Pacer control with engine-version-specific feature probes. Do not claim support based on Android generic setFrameRate alone.
- [ ] Trace OEM Fengchi game services to graphics superframe; Frame Sync produce/consume alone is *not* GPU interpolation.
- [ ] Verify at least one real Fengchi-enabled device and one alternate version/device, with stock features retained, CPU/GPU thermals, frame pacing and rollback.
- [ ] Reboot/uninstall/disabled-game fallback verified; no persistent game_opt/GEAS/HMBIRD mutation.
- [ ] Only after verified: versioned flashable stable release with source/CI/test artifacts.

## Improvements in this branch (still incomplete)
- Avoid legacy Screen.currentResolution icall whenever only targeting Unity FPS; this removes one incompatible API from the hot path.
- Try conventional and qualified Unity fps setter icall names.
- Read back Unity target FPS when getter resolves (proves an engine property, **not measured FPS**).
- Retry resolving startup IL2CPP icalls instead of permanently failing if Unity loads its library before registering icalls.
- Keep opcode rewriting disabled and OEM GPU + resolution untouched by default.

## What current CI can verify
C++ lookup fallbacks, source-level guard tests, Android ARM64 compilation and module packaging mechanics. None of these proves in-game unlock, refresh rate, smoothness, device safety, or OEM superframe operation.

References: Unity `Application.targetFrameRate` Android documentation; Unity staff explanation that arbitrary native thread Unity API calls are unsupported.
