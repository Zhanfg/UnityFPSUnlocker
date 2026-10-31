#!/system/bin/sh
# Fixed-entrypoint WebUI helper, never accepts arbitrary shell fragments.
MODDIR=${0%/*}
CLI="$MODDIR/fps-profilectl"
# Scan, capability discovery and COSA status are read-only and MUST still
# work even if a native profile helper is absent or cannot execute.
require_cli() {
  # v0.7.0 installation may have reset the binary to 0644.
  if [ -f "$CLI" ] && [ ! -L "$CLI" ] && [ ! -x "$CLI" ];then
    chmod 0755 "$CLI" 2>/dev/null || true
  fi
  if [ ! -f "$CLI" ];then
    echo "ERROR: profile helper file not installed: $CLI" >&2
    return 127
  fi
  if [ ! -x "$CLI" ];then
    echo "ERROR: profile helper is not executable: $CLI" >&2
    return 126
  fi
}
ACTION=${1:-}
case "$ACTION" in
  read) require_cli || exit $?; exec "$CLI" read ;;
  set)
    [ $# -eq 7 ] || exit 64
    require_cli || exit $?
    exec "$CLI" set "$2" "$3" "$4" "$5" "$6" "$7" ;;
  remove)
    [ $# -eq 2 ] || exit 64
    require_cli || exit $?
    exec "$CLI" remove "$2" ;;
  caps)
    for policy in /sys/devices/system/cpu/cpufreq/policy[0-9]*;do
      [ -d "$policy" ] || continue
      MIN=$(cat "$policy/cpuinfo_min_freq" 2>/dev/null)
      MAX=$(cat "$policy/cpuinfo_max_freq" 2>/dev/null)
      [ -n "$MIN" ] && [ -n "$MAX" ] || continue
      printf 'CPU|%s|%s|%s\n' "${policy##*/}" "$MIN" "$MAX"
    done
    for gpu in /sys/class/kgsl/kgsl-3d0/devfreq /sys/class/devfreq/*gpu*;do
      [ -f "$gpu/min_freq" ] || continue
      [ -f "$gpu/max_freq" ] || continue
      printf 'GPU|%s|%s|%s\n' "$gpu" "$(cat "$gpu/min_freq")" "$(cat "$gpu/max_freq")"
      break
    done
    ;;
  scan)
    # Probe both Android command entrypoints; some OEM WebUI shells return an
    # empty stdout for 'pm list packages -3' despite having installed games.
    PKGS=$(cmd package list packages -3 2>/dev/null) || PKGS=""
    if [ -z "$PKGS" ];then
      PKGS=$(pm list packages -3 2>/dev/null) || PKGS=""
    fi
    if [ -z "$PKGS" ];then
      echo "ERROR: Android package manager returned no user applications" >&2
      exit 75
    fi
    printf '%s\n' "$PKGS" | sed -n 's/^package://p' | while IFS= read -r pkg;do
      case "$pkg" in ''|*[!a-zA-Z0-9._]*) continue;;esac
      case "$pkg" in
        *game*|*unity*|*unreal*|com.miHoYo.*|com.HoYoverse.*|com.tencent.tmgp.*|com.netease.*|com.roblox.*|com.supercell.*|com.kurogame.*)
          printf 'GAME|%s\n' "$pkg";;
        *)
          # Do not do a full dumpsys for EVERY installed application: it can
          # stall the WebUI on modern ColorOS. Unknown still appears in All.
          printf 'APP|%s\n' "$pkg";;
      esac
    done
    ;;
  scrc-status)
    exec sh "$MODDIR/../scrc-bridge.sh" status ;;
  scrc-list)
    exec sh "$MODDIR/../scrc-bridge.sh" list ;;
  opgce-status)
    exec sh "$MODDIR/../opgce-bridge.sh" status ;;
  opgce-list)
    exec sh "$MODDIR/../opgce-bridge.sh" list ;;
  diagnose)
    [ "$#" -eq 2 ] || exit 64
    exec sh "$MODDIR/../game-diag.sh" "$2" ;;
  measure)
    [ "$#" -eq 2 ] || exit 64
    case "$2" in
      ''|.*|*..*|*[!a-zA-Z0-9._]*) echo "Invalid package" >&2;exit 64;;
    esac
    case "$2" in *.*) ;; *) echo "Invalid package" >&2;exit 64;; esac
    # Give the user time to switch from the manager WebUI to the game.
    # Sampling a backgrounded game would falsely report low/unavailable FPS.
    sleep 5
    exec sh "$MODDIR/../fps-evidence.sh" "$2" 2 ;;
  runtime) require_cli || exit $?; exec "$CLI" runtime ;;
  restore)
    sh "$MODDIR/../perf-guard.sh" restore || exit 75
    echo OK;;
  *)
    echo "Usage: profile-ui.sh read|scan|caps|measure PKG|set PKG FPS INTERVAL CPU_KHZ GPU_MHZ DELAY|remove PKG" >&2
    exit 64 ;;
esac
