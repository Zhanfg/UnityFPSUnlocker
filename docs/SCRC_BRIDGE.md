# SCRC v5.7 compatibility findings

Reviewed supplied SCRC-v5.7.zip without executing any installers, binaries or kernel modules.

## Implementation and distinction from OPGCE
- SCRC module id is `scrc`. `bin/inject` is a stripped Android aarch64 binary that updates COSA `PackageConfigBean` using dynamic SQLite. Its embedded SQL installs `protect_local_pkg_update`, `protect_local_pkg_insert`, `protect_local_pkg_delete` triggers. Consequently a second COSA injector may conflict at the database schema/record level; the real activation outcome cannot be inferred from presence of .enc files.
- `match_platform.sh` chooses configuration based on `ro.soc.model`. The supplied ZIP contains 49 SM8650 encrypted game profiles, one SM8750 (`com.miHoYo.Yuanshen.enc`), and two SM8845 profiles.
- Optional `ctn_patch.ko` has vermagic `6.1.141-android14-11-o-g7598c3221214` and augments old GameOpt `critical_task_name`. It is NOT compatible with unrelated kernels, including the OnePlus 13 6.6 series. `opgs_daemon` reads COSA and accesses `/proc/game_opt/task_boost/critical_task_name`; availability is device/kernel-specific.
- SCRC installer may enable OEM system services/properties and disable user experience collection; our bridge NEVER runs these scripts.
- SCRC installer registers `/data/adb/service.d/.scrc_cleanup.sh`: after the `scrc` module directory disappears and Android boot completes, that script runs `pm clear com.oplus.cosa`. This can erase COSA app data. Do not remove SCRC without independent database backup and understanding its cleanup behavior.
- SCRC's own README discourages simultaneous third-party scheduling modules, so **our own CPU/GPU sysfs frequency floors are suppressed for ALL games when an enabled SCRC module is detected**, not merely games with a matching .enc profile.

## Read-only SCRC bridge
- `packaging/scrc-bridge.sh` detects installed/enabled state, version, deployed `.enc`/`.json` profiles, OPGS proc node existence, kernel-family hint and whether OPGCE is also installed/enabled.
- The KernelSU/APatch WebUI shows SCRC candidate games, device compatibility information and a high-priority warning when **both SCRC and OPGCE** are enabled. It cannot prove a COSA profile is active just by reading filenames.
- `service.sh` automatically restores our owned minimum-frequency sysfs writes and yields scheduling when SCRC is enabled. Unity native FPS interception remains a separate user-configured feature.
- No SCRC binaries or proprietary encrypted profiles are bundled; no automatic injection, database migration, service restart, kernel insertion, cleanup or module uninstallation.

## Recommendations
1. On a device where OPGCE is already active, do not also start the SCRC COSA injector without a tested transaction/rollback plan for `PackageConfigBean` and its protection triggers.
2. On OnePlus 13 SM8750, supplied SCRC profile coverage is currently only `com.miHoYo.Yuanshen`. SM8650/SM8845 assets are not automatically compatible.
3. This read-only integration only prevents *our* frequency-floor conflicts. It does not prevent two independently installed injectors from changing COSA data. The WebUI warns but does not disable either module on the user's behalf.
4. SCRC's optional `ctn_patch.ko` requires exact kernel ABI verification, including symbol CRCs. The bridge does not load the addon.

Reference: https://github.com/xhai-git/SCRC (upstream warning and compatibility list).