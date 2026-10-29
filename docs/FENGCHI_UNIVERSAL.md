# Fengchi Universal: compatibility contract / 风驰通用兼容约定

## Support is based on runtime capabilities, NOT device names

The same experimental ARM64 module is intended for Fengchi-equipped OnePlus /
OPlus devices regardless of model, SoC generation, or ColorOS version.
**No SM8750, OnePlus 13, or 120 Hz device whitelist is compiled into it.**

The module never installs/replaces a kernel, GPU driver, DTBO, vendor service,
game frame generator, display mode or thermal policy. Zygisk injection itself
remains an opt-in risk for online games.

## What is actually detected (read-only)

Zygisk companion logs independent evidence from procfs/sysfs; the packaged
`fengchi-probe.sh` prints the same evidence for troubleshooting.

| Capability | Evidence path(s) | Meaning |
| --- | --- | --- |
| HMBIRD | `/proc/hmbird_sched`, `/sys/module/hmbird_sched` | scheduler interface hint |
| Game Opt | `/proc/game_opt`, `/sys/module/oplus_game_opt`, `/sys/module/oplus_game` | vendor game optimizer hint |
| Frame Sync | `/proc/game_opt/frame_sync`, `/proc/sys/game_frame_sync` | frame event coordination hint |
| Early Detect | `/proc/game_opt/early_detect` | frame timeout prediction hint |
| GEAS | `/proc/game_opt/geas_ctrl`, `/sys/module/oplus_geas` | vendor workload/bandwidth tuning hint |

These paths are extracted from publicly available OPlus kernel sources;
versions vary. Detection is **non-destructive** and deliberately not a
mandatory gate: kernels with no accessible procfs nodes are reported as
`unknown`, NOT classified as non-Fengchi. Presence also does not prove
the feature is active or that an OEM frame-generator is enabled.

OnePlusOSS reference source:
[game_opt](https://github.com/OnePlusOSS/android_kernel_modules_and_devicetree_oneplus_sm8750/tree/oneplus/sm8750_b_16.0.0_oneplus_13/vendor/oplus/kernel/cpu/game_opt)
and [sched_ext](https://github.com/OnePlusOSS/android_kernel_modules_and_devicetree_oneplus_sm8750/tree/oneplus/sm8750_b_16.0.0_oneplus_13/vendor/oplus/kernel/cpu/sched_ext).

## Distinguish native FPS, OEM superframe FPS, physical refresh (Hz)

- `fps` in `TargetList.json` is a **Unity IL2CPP native target FPS**, NOT an
  OEM interpolation/superframe target. This setting only works where Unity
  accepts it and the user explicitly opts in.
- OEM Fengchi superframe output is configured by the OEM's own graphics stack;
  this fork does NOT forge OEM frame event ioctls or silently activate it.
- The panel's current refresh Hz is yet another quantity. Do not clamp a
  *superframe-output target* to a panel's advertised refresh-rate value.
  Conversely, 144 frame-generation FPS alone does not prove 144 Hz scanning.
- Native target values 120, 144, 165, 185 are accepted by the policy, but
  are **not guarantees** and do not mean that OEM superframe at those rates
  is implemented by this project. The policy currently bounds native FPS
  to 30–240 as a validation guard; this is NOT a hardware frame limit.
- Keep `scale: -1.0` to leave OEM super-resolution untouched.
- Do not enable another interpolation/overclocking controller by default.

## Read-only device diagnostic

Once installed in a Magisk/KernelSU/APatch module path, execute (adjust folder
to the module ID when necessary):

```sh
su -c 'sh /data/adb/modules/unityfpsunlocker_oemsafe/fengchi-probe.sh'
```

Outputs independent presence flags and `classification`. In particular,
`oem_superframe_max_fps=unknown` is intentional: we have not verified
a universal, stable public API to read/set it on all Fengchi versions.
No write is performed to any vendor node.

## Test and maturity

- Policy tests and ARM64 Zygisk build are verified in GitHub Actions.
- Kernel feature detection is tested with simulated layouts, not on all devices.
- ColorOS/OxygenOS, MediaTek, diverse OPlus families, 144+ OEM superframe
  and online-game anti-cheat compatibility remain **unverified** in vivo.
- Branch is experimental. Never automatically activate games or merge it
  to the production branch based on CI alone.
