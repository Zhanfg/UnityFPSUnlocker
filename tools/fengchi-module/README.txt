Fengchi Direct Lab v0.3.0-alpha1

Install ZIP in KernelSU/Magisk/APatch module manager; not in recovery.
No boot-time scripts or driver overlays are included.
No game app is required. Root and ARM64 are required.

Commands (use with root shell):
 sh /data/adb/modules/fengchi_direct_lab/bin/fengchi status
 sh /data/adb/modules/fengchi_direct_lab/bin/fengchi ab 60 3
 sh /data/adb/modules/fengchi_direct_lab/bin/fengchi ab 144 3
 sh /data/adb/modules/fengchi_direct_lab/bin/fengchi early 60 3

Modes:
 status: read-only, shows game_opt and scheduler baseline.
 ab: native BAAB experiment, only modifies game_pid temporarily, sends frame_sync ioctl.
 early: experimental; requires game_pid=-1, ed_enable=0, target_fps=0 and default ed_args.
        Temporarily sets target_fps and Early Detect, emits synthetic frame_produce writes,
        and restores defaults on normal exit. CPU/GPU frequency snapshots are collected.

Early mode affects vendor GLOBAL state, not only the test process.
Do not run while playing. Root scripts cannot guarantee recovery after power loss,
SIGKILL or concurrent vendor writes. If a test is interrupted, check status.
No OEM GPU superframe (interpolation) implementation or general FPS unlocking here.
