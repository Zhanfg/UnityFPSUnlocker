# OPGCE v3.1.6 compatibility bridge

## Inspected source
User-provided OPGCE_v3.1.6(1).zip. No opaque inject_core or .enc binaries are copied into this repository.

- Module ID: cosa_injector, OP Gaming Core Enhancer v3.1.6.
- inject_cosa.sh runs a proprietary arm64 inject_core to update com.oplus.cosa/databases/db_game_database using encrypted per-SoC/per-game profiles.
- device_config.json maps canonical game package identifiers and regional aliases.
- The installer selects platform, model, SDK overlays and copies active .enc files into the encrypted_strategy root.
- /dev/opgce/mode is initialized and written by powercfg.sh to fast/powersave/balance/performance, but no verified dynamic kernel scheduling change from a simple mode write is demonstrated. Read only.
- The included uninstall.sh calls pm clear com.oplus.cosa and removes application data directories. Do NOT import or call this script from UnityFPSUnlocker. Back up COSA before manual uninstall.
- customize.sh references sgame_mtr.sh, but that optional file is absent from the supplied ZIP.

## Bridge implementation
1. Detect OPGCE installed and enabled state at /data/adb/modules/cosa_injector; disabled/removed modules do not count.
2. Read deployed root encrypted_strategy/*.enc filenames and alias metadata only. This is compatibility detection, not proof that a COSA profile applied.
3. WebUI displays OPGCE version, mode marker (display only), deployed profile count and candidate game badges.
4. For a running game matching an enabled deployed OPGCE profile or its alias, our separate CPU/GPU floor service restores its own changes and defers frequency control to OPGCE. Unity Zygisk FPS handling remains independent.
5. When there is no active OPGCE match, standalone CPU/GPU floors remain available by opt-in configuration.
6. Bridge does not execute inject_core, alter COSA database, modify /dev/opgce/mode, or decrypt/copy any OPGCE strategy.

## Limitations
- Supported SoC directory (e.g. SM8750) does not prove the device has a model-specific profile or successful injection.
- Only current deployed .enc profiles are treated as active. Games recognized solely from a root strategy filename or alias are not guaranteed to be active in COSA.
- More than one resident game process, FPS overlay/thermal caps and ColorOS version differences require actual device testing.
- Test: tests/test_opgce_bridge.sh simulates enabled/disabled modules and aliases, verifies nonexecution of a mock injector.
- Important: when uninstalling the supplied OPGCE module, its own script may delete COSA game-service data. UnityFPSUnlocker will never invoke its uninstall path.
