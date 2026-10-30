# Fengchi Direct A/B Experiment (v2)

This experiment runs on Android AArch64 under native root shell. It does **not** start a game or inject into another application's process.

## Observed source facts
- OPlus `game_opt/task_util.c` accepts an existing *process leader* PID through `/proc/game_opt/game_pid`. The test only registers itself when the initial value is `-1`.
- `game_opt/frame_sync.c` accepts `GAMEOPT_EPOLL_PRODUCE` and `GAMEOPT_EPOLL_CONSUME` ioctls on `/proc/game_opt/frame_sync/sync_ctrl`.
- The Produce handler invokes `cl_notify_frame_produce()`, `fl_notify_frame_produce()`, `htb_notify_frame_produce()`, `ctb_notify_frame_produce()`, and may invoke the GEAS frame bandwidth event callback when enabled.
- **Important:** the `early_detect/frame_produce` proc writer is a **separate entry point**. It is NOT automatically called by `sync_ctrl` in the examined vendor code. The A/B test intentionally does not modify `ed_enable`, `target_fps`, `flt_enable`, GEAS or HMBIRD.
- Frame Sync ioctl success measures acceptance of a kernel event. No GPU frame rendering, OEM interpolation or super-resolution is performed.

Examined vendor source branch: `OnePlusOSS/android_kernel_modules_and_devicetree_oneplus_sm8750`, `oneplus/sm8750_b_16.0.0_oneplus_13`, under `vendor/oplus/kernel/cpu/game_opt`.

## Measurement procedure

The native executable `fengchi_ab_arm64` uses the same process and the same amount of CPU work per simulated frame in all phases.

| Phase | CPU work | Native game_opt Produce/Consume ioctl |
|---|---|---|
| B1 | yes | no |
| A1 | yes | yes |
| A2 | yes | yes |
| B2 | yes | no |

The order B-A-A-B reduces drift from CPU frequency ramp and thermal conditions, but cannot remove all confounders. Every tenth of a second it samples CPU frequency and available KGSL GPU frequency. Shell script collects OEM proc nodes before, during and after.

`--fps 60 --seconds 3 --busy-us 2500 --csv /data/local/tmp/your.csv` is a representative test invocation. Only a user-controlled private path should be used as the CSV destination.

The helper exits rather than override an occupied game PID. Its exit handler sends STOP and releases `game_pid` only if still owned. The wrapper adds a watchdog cleanup path. SIGKILL/power loss, or concurrent vendor writes, remain inherently risky; never test while a game is active.

## Interpretation

Compare CPU6/CPU7 frequency averages and the raw samples from **A1+A2** against **B1+B2** while checking frame ioctl failures and snapshots. CPU clock differences alone do NOT prove a vendor rendering effect, a frame-rate unlock or a graphics boost. Repeat tests at both 60 and 144 synthetic Hz when safe.

## Build

`.github/workflows/fengchi-native-selftest.yml` builds the helper using Android NDK r27c, API 28. No Termux is needed on the phone. The single-file self-extracting shell wrapper is separately packaged with the ELF payload and verified SHA-256.

Experimental, read-only except for *temporary self PID registration and ioctls*; no persistent OEM tuning changes are intentional.
