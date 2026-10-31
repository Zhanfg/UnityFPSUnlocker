# Unity FPS Unlocker WebUI (v0.7.0-dev.hotfix2)

**Development build**: KernelSU / KernelSU-Next / APatch manager WebUI. Android ARM64 + Zygisk required for Unity native FPS interception. Not a verified general-purpose UE or Fengchi frame-generation unlocker.

## Install and open
1. Flash the module ZIP in the Root manager. Reboot to start Zygisk and the optional performance service.
2. Open the module details → **WebUI** (KernelSU and APatch WebUI-capable versions).
3. The interface automatically scans `pm list packages -3`; Android's `CATEGORY_GAME` metadata and common naming patterns are used for **candidates only**. Use **All Apps** or manual package-name entry for misclassified games. Where available, the manager's package-info API supplies local app labels.
4. Select an app, choose FPS (0=do not inject, 30–240) and optional `render_interval=1`, CPU min MHz, GPU min MHz, then **Save**.
5. Restart the selected game to apply the Zygisk FPS interception. The performance guard applies only to a currently running configured app and restores its own frequency-floor values when that app closes or profile changes.
6. **Restore frequencies** button forcibly restores only frequencies which still match the last values written by this module.

## Persistent profile storage
- `/data/adb/ufps-webui/TargetList.json`, root-owned 0600; parent 0700
- Previously saved `/data/local/tmp/TargetList.json` migrated *once* on upgrade (if the existing path is a regular file).
- Native `fps-profilectl` validates package names and FPS/frequency ranges, and saves via atomic rename. It does not blindly replace an invalid existing configuration.
- For direct Root Shell: `sh /data/adb/modules/unityfpsunlocker_oemsafe/bin/profile-ui.sh read`. WebUI is optional; no Termux required.

## Frequency policy / 频率控制
- CPU input = minimum frequency in MHz, converted to the kernel's CPU cpufreq kHz field. On each CPU policy, only frequencies within the policy's current min/max and `scaling_available_frequencies` are considered.
- GPU input = minimum frequency in MHz, converted according to the GPU devfreq sysfs units. Requires `available_frequencies` and a supported min/max range. Unsupported frequencies are **skipped**, not overclocked.
- Defaults 0: do not change any clocks. No changes to CPU/GPU *maximum* clocks, kernel voltage, thermal limits, or OEM GameOpt/GEAS/HMBIRD.
- Higher-than-published GPU or CPU frequencies require hardware/driver/kernel-level work and **are not available through this WebUI**.
- Performance settings can cause heat, throttling and battery use; existing performance modules may compete for the same sysfs fields. The guard restores a value only if it has not been superseded by another writer.
- Per-game monitoring is based on the main package process, not game scene or foreground classification. Closing the game or rebooting removes active floors. Killing the service may leave a floor until reboot or manual Restore.

## Remaining limitations
- Unity setter Hook installation and target readback do not prove *rendered* FPS. Use `fps-evidence.sh` and SurfaceFlinger timestamps to verify.
- Some games set their cap before the Hook is installed or use other render-pacing mechanisms.
- An app may reject the target FPS regardless of the WebUI config. UE4/UE5 and genuine Fengchi OEM GPU interpolation are still unimplemented.
- WebUI uses native `ksu.exec` bridge; manager implementations without it need manager support rather than a remote WebView.

## Commands and privacy
All WebUI operations are local. The app list and labels are never transmitted externally. No server, tracking, CDN, user token, or third-party configuration service is required. Files are stored on-device with root permissions.

## OPGCE compatibility

See [OPGCE_BRIDGE.md](OPGCE_BRIDGE.md). Installed OPGCE strategies are detected read-only and suppress this module's competing CPU/GPU frequency floors for matched games. This does not copy encrypted game strategies, call inject_core or guarantee real FPS.

## SCRC compatibility

See [SCRC_BRIDGE.md](SCRC_BRIDGE.md). SCRC metadata is read only. If SCRC is enabled, our CPU/GPU frequency-floor writes are suspended for all games as the upstream requires no competing scheduling modifications. If both SCRC and OPGCE are enabled, the interface warns about two competing COSA injectors; it does not attempt to disable or repair either one.

## Testing FPS from the WebUI

1. Select the configured game, launch that game, then return to the WebUI.
2. Press **采集游戏实际帧率**. It uses KernelSU's asynchronous exec callback, collects three read-only SurfaceFlinger presentation samples and writes an archive into `/sdcard/Download` when writable (otherwise `/data/local/tmp`).
3. Inspect `surface_present_fps`, valid samples, median/p95 present intervals, and separately verify Unity native `HIT/NO_HIT`. These measurements do not prove physical panel frequency or OEM interpolation.
4. FPS setting is read anew by the Zygisk companion for **each new game process**; after saving, fully exit and restart the target game. A corrupted config fails closed instead of silently continuing with stale settings.

The app scan also uses the asynchronous manager callback where supported, to avoid freezing the WebUI while PackageManager evaluates game categories.

## v0.7.0-dev.hotfix1: WebUI 'profile helper missing' on KernelSU

The v0.7.0 ZIP contained `bin/fps-profilectl`, but did **not** contain a `customize.sh`; KernelSU's installer may reset the native binary to mode `0644`, making the earlier WebUI `-x` guard report `ERROR: profile helper missing`. The fix adds an install-time `customize.sh` which invokes `set_perm` on all executable scripts and the ARM64 helper after extraction. The WebUI bridge also attempts a one-time owner-only `chmod 0755` on a present but nonexecutable binary, reporting missing files separately from wrong permissions. CI now simulates KernelSU stripping all permissions before sourcing customize.sh and tests WebUI helper lookup.

If you are already running the affected v0.7.0 build, execute in a native Root Shell (no Termux required):

```sh
MOD=/data/adb/modules/unityfpsunlocker_oemsafe
ls -l "$MOD/bin/fps-profilectl"
chmod 0755 "$MOD/bin/fps-profilectl"
"$MOD/bin/fps-profilectl" read
```

If the last line prints JSON, restart the module WebUI. If the file is absent, reinstall the complete ZIP and reboot. If the file exists but execution fails with `Permission denied` or `not found`, collect the *exact* error and `ls -lZ` for SELinux/loader diagnosis instead of assuming another cause. This hotfix does not claim actual in-game FPS increases.

## hotfix2: 0 apps and manually set FPS not taking effect

1. Game scanning now prefers native manager APIs `listUserPackages()` or `listPackages("user")`, then `listAllPackages()`, independent of the Root Shell `pm` path. Fallback uses `cmd package list packages -3` and then `pm list packages -3`.
2. UI initially lists all apps; package naming and GAME metadata are only recommendations. Non-game-labelled Android apps are selectable.
3. Saving a per-app profile now reads back `TargetList.json` and verifies all user-set values. If persistence fails the UI reports an error; saving does not mean FPS unlocking.
4. For a selected, running game press **检查游戏注入状态**. The read-only output includes installation lookup, PID, IL2CPP library presence, module state and last tagged hook logs. Recent module logs may belong to other processes; inspect PIDs and timestamps.
5. `NOT_ARMED` means Unity FPS icall was not available, `ARMED` means interception installed but not necessarily invoked, `NO_HIT` means no game setter call occurred during the 8 second observation, and `HIT` means a game-originated native setter call was intercepted. Even HIT does not prove an actual rendered FPS increase.
6. Save the target FPS, fully close the game, relaunch it, and run diagnostics again. If the game is not Unity IL2CPP or sets FPS before our hook, this Unity-only native setter path cannot unlock it. UE and OEM interpolation are separate work.

**Safety:** No SCRC or OPGCE database modification, no kernel overclock, no thermal disablement. Unknown app identity is not proof that the FPS intervention succeeded.
