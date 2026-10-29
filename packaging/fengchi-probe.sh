#!/system/bin/sh
# Read-only, device-independent OEM game-kernel capability diagnostics.
# Does not touch any sched, sysfs, GPU, frame-generation or display setting.
# FENGCHI_PROBE_ROOT is for offline tests only.
ROOT=${FENGCHI_PROBE_ROOT:-/}
ROOT=${ROOT%/}
[ -n "$ROOT" ] || ROOT=""

has_path() {
  [ -e "$ROOT$1" ] || [ -d "$ROOT$1" ]
}

flag() {
  if has_path "$1"; then
    echo 1
  else
    echo 0
  fi
}

if has_path /proc/hmbird_sched || has_path /sys/module/hmbird_sched; then
  hmbird=1
else
  hmbird=0
fi

if has_path /proc/game_opt || has_path /sys/module/oplus_game_opt || has_path /sys/module/oplus_game; then
  game_opt=1
else
  game_opt=0
fi

if has_path /proc/game_opt/frame_sync || has_path /proc/sys/game_frame_sync; then
  frame_sync=1
else
  frame_sync=0
fi

early_detect=$(flag /proc/game_opt/early_detect)

if has_path /proc/game_opt/geas_ctrl || has_path /sys/module/oplus_geas; then
  geas=1
else
  geas=0
fi

# A hint, not a certificate: some firmware hides these interfaces.
classification=unknown
if [ "$game_opt" = 1 ] && { [ "$hmbird" = 1 ] || [ "$frame_sync" = 1 ] || [ "$geas" = 1 ]; }; then
  classification=oem_game_stack_candidate
elif [ "$hmbird" = 1 ] || [ "$game_opt" = 1 ] || [ "$geas" = 1 ]; then
  classification=partial_interfaces
fi

printf 'probe_version=1\n'
printf 'hmbird=%s\n' "$hmbird"
printf 'game_opt=%s\n' "$game_opt"
printf 'frame_sync=%s\n' "$frame_sync"
printf 'early_detect=%s\n' "$early_detect"
printf 'geas=%s\n' "$geas"
printf 'classification=%s\n' "$classification"
printf 'oem_superframe_active=unverified\n'
printf 'oem_superframe_max_fps=unknown\n'
printf 'physical_refresh_hz=not_probed\n'
