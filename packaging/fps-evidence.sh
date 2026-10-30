#!/system/bin/sh
# Read-only, no-Termux FPS evidence capture for an already running game.
# Does not start apps, alter display frequency or write OEM kernel controls.
umask 077
set -u
MODDIR=${0%/*}
APP=${1:-}
DURATION=${2:-2}
case "$APP" in
  ''|*[!a-zA-Z0-9._]*)
    echo "Usage: sh $0 com.example.game [seconds 1..5]" >&2;exit 64;;
esac
case "$DURATION" in
  ''|*[!0-9]*) echo "ERROR duration must be an integer" >&2;exit 64;;
esac
[ "$DURATION" -ge 1 ] && [ "$DURATION" -le 5 ] || { echo "ERROR 1..5s" >&2;exit 64; }
[ "$(id -u)" = 0 ] || { echo "ERROR root required" >&2;exit 77; }
command -v dumpsys >/dev/null 2>&1 || { echo "ERROR dumpsys unavailable" >&2;exit 72; }
PIDS=$(pidof "$APP" 2>/dev/null || true)
[ -n "$PIDS" ] || { echo "ERROR game not running: $APP" >&2;exit 73; }
STAMP=$(date +%Y%m%d_%H%M%S)
WORK="/data/local/tmp/FPS_Evidence_${STAMP}_$$"
mkdir -m 700 "$WORK" || exit 70
OUT=/sdcard/Download
[ -d "$OUT" ] && [ -w "$OUT" ] || OUT=/data/local/tmp
{
  echo "mode=READ_ONLY"
  echo "application=$APP"
  echo "game_pids=$PIDS"
  echo "kernel=$(uname -r)"
  echo "android=$(getprop ro.build.version.release 2>/dev/null)"
  echo "sdk=$(getprop ro.build.version.sdk 2>/dev/null)"
  echo "capture_started=$STAMP"
} > "$WORK/metadata.txt"
dumpsys SurfaceFlinger --list 2>/dev/null | grep -F "$APP" > "$WORK/layers.txt" || true
LAYERS=$(grep 'SurfaceView' "$WORK/layers.txt" || true)
N=$(printf '%s\n' "$LAYERS" | grep -c '.' || true)
echo "surfaceview_matches=$N" >> "$WORK/metadata.txt"
if [ "$N" = 1 ]; then
  LAYER=$LAYERS
  echo "layer=$LAYER" >> "$WORK/metadata.txt"
  i=0
  while [ "$i" -lt 3 ]; do
    dumpsys SurfaceFlinger --latency "$LAYER" > "$WORK/latency_$i.txt" 2>&1 || true
    sh "$MODDIR/sf-present-fps.sh" --file "$WORK/latency_$i.txt" > "$WORK/fps_$i.txt" 2>&1 || true
    i=$((i+1))
    if [ "$i" -lt 3 ]; then sleep "$DURATION"; fi
  done
else
  echo "ERROR: zero or multiple SurfaceView layers; latency collection skipped" > "$WORK/fps_0.txt"
fi
# Our own log tag only: do not collect the user's general logcat.
logcat -d -s UnityFPSUnlocker:D UnityFPSUnlocker:E > "$WORK/hook_log.txt" 2>&1 || true
{
  echo "present_display"
  dumpsys display 2>/dev/null | grep -E 'mActiveMode|mRefreshRate|fps=|refreshRate=' | head -n 60 || true
  echo "kernel_game_pid"
  if [ -r /proc/game_opt/game_pid ]; then cat /proc/game_opt/game_pid;else echo unavailable;fi
  echo "early_detect_target"
  if [ -r /proc/game_opt/early_detect/target_fps ]; then cat /proc/game_opt/early_detect/target_fps;else echo unavailable;fi
} > "$WORK/display_kernel.txt"
NAME="FPS_Evidence_${STAMP}_$$.tar.gz"
if tar -czf "$OUT/$NAME" -C "$WORK" .; then
  echo "Archive: $OUT/$NAME"
  echo "Surface matching count: $N"
  cat "$WORK/fps_0.txt"
else
  echo "Archive failed; raw logs $WORK" >&2
  exit 75
fi
