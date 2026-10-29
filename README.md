# UnityFPSUnlocker — OEM-preserving Zygisk fork (experimental)

> Upstream: [hexstr/UnityFPSUnlocker](https://github.com/hexstr/UnityFPSUnlocker), MIT License.
> This fork's **feature/oem-fengchi-passive-fps-v1** is an experimental cross-device Fengchi-aware iteration.
> CI produces a flashable ARM64 test ZIP. It has **not** been verified on real Fengchi devices.

## Device-independent Fengchi support / 跨设备风驰支持

- No OnePlus 13 / SM8750 hardcode. Detect separate OPlus scheduler, game-opt,
  frame-sync, early-detect and GEAS interfaces at runtime, read-only.
- Interfaces vary by device/firmware. Probe output is a *hint*, not a promise
  that OEM Fengchi superframe is active or a specific frame rate is supported.
- **144 FPS OEM superframe is not the same as 144 Hz panel refresh.** Do not
  impose a 120 cap based only on the official panel specification.
- 144, 165 and 185 are valid *native FPS target requests* in the allowlist;
  no OEM frame generation API has been implemented in this fork.
- Full details: [FENGCHI_UNIVERSAL.md](docs/FENGCHI_UNIVERSAL.md).

## Goal / 目标

Use Unity IL2CPP's native `Application.set_targetFrameRate` only when the user
explicitly selects a game. **Do not replace the OPlus/OnePlus Fengchi game engine,
ColorOS super-frame/super-resolution processing, GPU driver, scheduler or touch pipeline.**
This is a complementary FPS target adjustment, not a replacement for the OEM rendering stack.

**Zygisk does not prevent anti-cheat detection.** Even with opcode modification disabled,
modifying a game's FPS may violate game rules. Prefer the official in-game/OEM FPS
setting on protected online titles. This branch adds no anti-cheat bypass.

## What changed / 此分支改动

- No global game injection by default: the default FPS is **0**.
- Removed automatic activation via `/sdcard/Android/data/<package>/files/il2cpp`.
  Only explicitly named processes in `custom` with valid FPS (30–240) can run.
- Native FPS opcode patching is **always disabled in the FPSLimiter path**, even
  if a legacy config requests `mod_opcode: true`.
- Resolution is left **unchanged by default** (`scale: -1.0`).
  When a user explicitly supplies a supported scale, the code calls Unity's
  resolution function but **does not patch that function to return immediately**.
- Default module behavior does not modify Android display modes, OPlus
  services, system properties, GPU drivers, kernel scheduling or touch settings.
- Added a small testable C++ policy and GitHub Actions regression checks.

## Configure / 配置

As with the original project, place a valid `TargetList.json` in
`/data/local/tmp/TargetList.json` before using a built module.

The repository's default configuration performs **no FPS adjustment**:

```json
{
  "global": {
    "delay": 8,
    "mod_opcode": false,
    "fps": 0,
    "scale": -1.0
  },
  "custom": {}
}
```

To **explicitly opt in** for a game you have permission to modify, add a process
name under `custom` and specify a valid `fps`:

```json
{
  "global": {
    "delay": 8,
    "mod_opcode": false,
    "fps": 0,
    "scale": -1.0
  },
  "custom": {
    "com.example.offlineunitygame": {
      "fps": 120,
      "delay": 10
    }
  }
}
```

The example process name is a placeholder; replace it with your own game.
An entry with no FPS (or with `fps: 0`) remains inactive.
`scale: -1.0` keeps the OEM resolution unchanged.
Explicit scaling is experimental and may interfere with the game's resolution
handling or OEM super-resolution; leave it disabled for normal gaming.

**120 / 144 / 165 / 185 native FPS are requests, not guarantees.** Actual rendered frames and displayed
frames depend on the game, OEM pipeline, thermal limits and refresh modes.
Do not combine it with separate frame-interpolation modules without testing.

## Development / 开发检查

```sh
python3 tests/check_safe_defaults.py
c++ -std=c++17 -Wall -Wextra -Werror -pedantic tests/oem_policy_test.cpp -o /tmp/oem_policy_test
/tmp/oem_policy_test
```

The policy CI checks default safety and synthetic Fengchi interface layouts.
The separate Android ARM64 CI compiles a Zygisk library and packages a test ZIP.
Neither CI proves OEM superframe functionality or real-device compatibility.

Run the read-only capability probe after installation:

```sh
su -c 'sh /data/adb/modules/unityfpsunlocker_oemsafe/fengchi-probe.sh'
```

The output does not enable any OEM feature, and missing nodes do not prove
Fengchi is absent.

## Upstream and license

Copyright and original implementation belong to the respective contributors
of [hexstr/UnityFPSUnlocker](https://github.com/hexstr/UnityFPSUnlocker).
Original project documentation: [README_en.md](README_en.md) / [README_jp.md](README_jp.md).
See [LICENSE](LICENSE). Configuration and behavior in this branch intentionally
differ from the upstream release.
