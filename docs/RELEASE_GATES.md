# Unified FPS + Fengchi: release gates

**Not ready for user module delivery.** Keep current code as a development branch, even when ARM64 CI passes. The native Fengchi Direct Lab ZIP is a kernel-interface research tool, not a universal FPS unlocker.

## Known blockers / 尚未完成
- [x] Implement game-originated Unity native FPS setter interception: hook forwards to original setter on its caller's thread, with strict package allowlist. Still needs live-game execution trace to validate it is the correct Unity thread.
- [ ] Verify setter is reached for a real game's initial and repeated frame cap updates; games which never call the setter after hook installation are not yet handled.
- [ ] Validate live Unity IL2CPP FPS reassertion after scene switches, pause/resume, QualitySettings changes and runtime FPS setting changes.
- [x] Add a default-read-only OnDemandRendering static field probe and explicit per-game opt-in interval=1; validate IL2CPP field type/flags before any read/write. Needs live Unity version compatibility tests.
- [ ] Verify OnDemandRendering behavior in real gameplay, including intentional menu/background throttling and versions without the field.
- [x] Implement no-hit logging, setter-call count and a read-only SurfaceFlinger layer presentation estimator with offline 60/120 FPS fixtures.
- [ ] Verify requested target against real game engine/actual frame presentation and native display refresh separately, across at least two Unity versions.
- [ ] Implement and test UE4/UE5 Frame Pacer control with engine-version-specific feature probes. Do not claim support based on Android generic setFrameRate alone.
- [ ] Trace OEM Fengchi game services to graphics superframe; Frame Sync produce/consume alone is *not* GPU interpolation.
- [ ] Verify at least one real Fengchi-enabled device and one alternate version/device, with stock features retained, CPU/GPU thermals, frame pacing and rollback.
- [ ] Reboot/uninstall/disabled-game fallback verified; no persistent game_opt/GEAS/HMBIRD mutation.
- [ ] Only after verified: versioned flashable stable release with source/CI/test artifacts.

## Improvements in this branch (still incomplete)
- Avoid legacy Screen.currentResolution icall whenever only targeting Unity FPS; this removes one incompatible API from the hot path.
- Try conventional and qualified Unity fps setter icall names.
- Log readback inside game-originated setter interception when the getter resolves. **This is an engine property, not measured FPS.**
- Retry resolving startup IL2CPP icalls instead of permanently failing if Unity loads its library before registering icalls.
- For explicitly selected games only, hook Unity's native target-frame setter using a statically linked, pinned Dobby backend; keep the original function trampoline and intercept repeated calls. If no game setter call occurs after installation, the module cannot claim a cap change.
- Zero default injection delay for *new* installations; existing `/data/local/tmp/TargetList.json` remains user-owned and is never overwritten on upgrade.
- Keep opcode rewriting disabled and OEM GPU + resolution untouched by default.

## What current CI can verify
C++ lookup fallbacks, forced/disabled native FPS policy, source-level guard tests, ARM64 compilation and module packaging mechanics. A mock native setter trampoline integration test is also part of CI; it is not a Unity game test. None of these proves in-game unlock, refresh rate, smoothness, device safety, or OEM superframe operation.

References: Unity `Application.targetFrameRate` Android documentation; Unity staff explanation that arbitrary native thread Unity API calls are unsupported.

## Current implementation limits
- Hooking the Unity engine native setter changes the *requested* FPS value passed through that specific path. It does not modify OEM frame-generation, Android Game Mode, SurfaceFlinger, Swappy or other frame-pacing caps.
- Unity `OnDemandRendering.renderFrameInterval` is managed script state; altering `Application.targetFrameRate` cannot universally disable it.
- The Dobby hook changes executable code via a managed trampoline; unlike the earlier unsafe NOP approach, it forwards to the original function but still carries engine-integrity and runtime compatibility risks.
- Do not advertise a public module based solely on CI pass; complete real-engine and rendering measurements first.

## October 30 development additions
- Opt-in Unity native setter hook now reports explicit `NO_HIT` after an 8-second observation window instead of conflating installation with a game-request intercept.
- `sf-present-fps.sh` and `sf_latency_parser.awk` provide read-only SurfaceFlinger surface frame-time samples; no-game-data is unavailable, not a passing result.
- Reject OnDemand IL2CPP field modification unless metadata confirms `static int32`.
- Development module metadata and CI artifacts are labeled **UNVERIFIED**, with no stable release.
- Source: [FRAME_RATE_EVIDENCE.md](FRAME_RATE_EVIDENCE.md).
