# FPS evidence protocol (development only)

The runtime hook changes the **game's native Unity Application.targetFrameRate setter input**. It cannot claim a display FPS unlock until the below counters and presentation data are inspected.

## Three independent metrics
1. **Unity native setter interception**: `logcat -d -s UnityFPSUnlocker:D UnityFPSUnlocker:E`. `Unity setter interception call=N game=X override=Y forwarded=Y readback=Z` proves that calls to this native setter were intercepted. `NO_HIT after 8s` means that the hook was armed but no game-originated setter invocation was observed, **not** that the game ran at Y FPS.
2. **Surface presentation rate**: The read-only `sf-present-fps.sh --package com.example.game` helper lists a matching `SurfaceView` layer and samples `dumpsys SurfaceFlinger --latency <layer>`. It reports the recent layer-presentation timestamps (column B), span, median/p95 frame spacing, nominal refresh period and discarded fences. When multiple surfaces match, it fails closed and requires an explicit `--layer`. This measures **surface presentation, not Unity simulation FPS**.
3. **Physical panel refresh and OEM-generated frames**: inspect the device's display mode and OEM graphics pipeline separately. A display at 120 Hz cannot physically present 144 distinct visible frames per second. GPU interpolation can create output frames not present in Unity simulation counters; the observer cannot infer their provenance.

## Interpreting results
- `HOOK_NO_HIT`: game set its cap before the interceptor was ready, used another setter path, or never set it dynamically. **Not unlocked**.
- Hook HIT with `readback != configured target`: the desired Unity FPS state was not retained. **Not unlocked**.
- Hook HIT/readback equal desired, surface present FPS stays at 30/60: another pacing/display/OnDemand/OS constraint is active. **Not unlocked**.
- Hook HIT/readback equal desired, surface present FPS rises: the **presentation rate** has changed, but engine gameplay timing, interpolation, graphics correctness, thermals and multi-scene behavior require validation.
- SF latency unsupported/empty: result **unavailable**. Never substitute FPS=0 or target FPS.

## Root-shell usage (development build only)
```sh
sh /data/adb/modules/unityfpsunlocker_oemsafe/sf-present-fps.sh --package com.example.game
sh /data/adb/modules/unityfpsunlocker_oemsafe/sf-present-fps.sh --layer 'exact SurfaceFlinger layer from --list'
```

The helper performs **read-only** SurfaceFlinger queries and does **not** write `/proc/game_opt`, change HMBIRD, GEAS, thermals, screen Hz or GPU configuration. The fallback parser works on a saved `dumpsys SurfaceFlinger --latency <layer>` text file using `--file PATH`.

## Remaining release blockers
- No verified live Unity APK yet; no evidence across real game startup and multi-scene frame setter changes.
- Unity can impose pacing from native engine internals, QualitySettings, `OnDemandRendering.renderFrameInterval` and display refresh.
- No UE4/UE5 adapter has passed a game test.
- No OEM Fengchi graphics interpolation ABI has been identified or activated from production user space.
- Do not distribute development ZIP as a finished FPS unlocker.

Sources: https://developer.android.com/topic/performance/issues/slow-session
and https://android.googlesource.com/platform/external/chromium_org/+/d0247b1/build/android/pylib/perf/surface_stats_collector.py .
