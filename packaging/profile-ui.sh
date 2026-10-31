#!/system/bin/sh
# Fixed-entrypoint WebUI helper, never accepts arbitrary shell fragments.
MODDIR=${0%/*}
CLI="$MODDIR/fps-profilectl"
[ -x "$CLI" ] || { echo "ERROR: profile helper missing" >&2;exit 127; }
ACTION=${1:-}
case "$ACTION" in
  read) exec "$CLI" read ;;
  set)
    [ $# -eq 7 ] || exit 64
    exec "$CLI" set "$2" "$3" "$4" "$5" "$6" "$7" ;;
  remove)
    [ $# -eq 2 ] || exit 64
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
    # All user packages; heuristic recommendations, never hide other apps.
    pm list packages -3 2>/dev/null | sed -n 's/^package://p' | while IFS= read -r pkg;do
      case "$pkg" in ''|*[!a-zA-Z0-9._]*) continue;; esac
      case "$pkg" in
        *game*|*unity*|*unreal*|com.miHoYo.*|com.HoYoverse.*|com.tencent.tmgp.*|com.netease.*|com.roblox.*|com.supercell.*)
          printf 'GAME|%s\n' "$pkg";;
        *)
          # CATEGORY_GAME=0 in Android ApplicationInfo. OEM dumpsys output
          # varies; absence is unknown, so still expose the app for selection.
          if dumpsys package "$pkg" 2>/dev/null | grep -Eq 'category=0([^0-9]|$)|category: GAME|android.intent.category.GAME';then
            printf 'GAME|%s\n' "$pkg"
          else
            printf 'APP|%s\n' "$pkg"
          fi;;

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
  runtime) exec "$CLI" runtime ;;
  restore)
    sh "$MODDIR/../perf-guard.sh" restore || exit 75
    echo OK;;
  *)
    echo "Usage: profile-ui.sh read|scan|caps|measure PKG|set PKG FPS INTERVAL CPU_KHZ GPU_MHZ DELAY|remove PKG" >&2
    exit 64 ;;
esac
