# Unity FPS Unlocker WebUI (v0.7.0-dev)

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
