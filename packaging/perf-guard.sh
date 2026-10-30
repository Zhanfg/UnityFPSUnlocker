#!/system/bin/sh
# Opt-in, per-running-game CPU/GPU frequency floors. Never exceed kernel max.
# All sysfs writes are within currently published frequency ranges.
ROOT=${0%/*}
STATE=/data/local/tmp/.ufps_performance_state
ACTIVE=/data/local/tmp/.ufps_active_game
restore() {
  [ -f "$STATE" ] || { rm -f "$ACTIVE";return; }
  while IFS='|' read -r node previous applied;do
    [ -f "$node" ] || continue
    now=$(cat "$node" 2>/dev/null)
    if [ "$now" = "$applied" ]; then printf '%s\n' "$previous" > "$node" 2>/dev/null || true;fi
  done < "$STATE"
  rm -f "$STATE" "$ACTIVE"
}
safe_floor() {
  node=$1 requested=$2 low=$3 high=$4 list=$5
  [ -f "$node" ] && [ -w "$node" ] || return
  case "$requested:$low:$high" in *[!0-9:]*|'') return;;esac
  [ "$requested" -gt 0 ] && [ "$requested" -ge "$low" ] && [ "$requested" -le "$high" ] || return
  # If available frequencies are published, reject unsupported intermediate values.
  if [ -r "$list" ]; then
    grep -qw "$requested" "$list" 2>/dev/null || return
  else
    return
  fi
  previous=$(cat "$node" 2>/dev/null)
  case "$previous" in ''|*[!0-9]*) return;; esac
  [ "$requested" -gt "$previous" ] || return
  if printf '%s\n' "$requested" > "$node" 2>/dev/null;then
    printf '%s|%s|%s\n' "$node" "$previous" "$requested" >> "$STATE"
  fi
}
apply() {
  pkg=$1 cpukhz=$2 gpumhz=$3
  case "$cpukhz:$gpumhz" in *[!0-9:]*|'')return;;esac
  # Cleanly revert only the values we installed before starting a new game.
  restore
  printf '%s\n' "$pkg" > "$ACTIVE"
  if [ "$cpukhz" -gt 0 ];then
    for policy in /sys/devices/system/cpu/cpufreq/policy[0-9]*;do
      [ -d "$policy" ] || continue
      low=$(cat "$policy/cpuinfo_min_freq" 2>/dev/null)
      max=$(cat "$policy/scaling_max_freq" 2>/dev/null)
      case "$low:$max" in *[!0-9:]*|'') continue;;esac
      safe_floor "$policy/scaling_min_freq" "$cpukhz" "$low" "$max" "$policy/scaling_available_frequencies"
    done
  fi
  if [ "$gpumhz" -gt 0 ];then
    for gpu in /sys/class/kgsl/kgsl-3d0/devfreq /sys/class/devfreq/*gpu*;do
      [ -f "$gpu/min_freq" ] || continue
      max=$(cat "$gpu/max_freq" 2>/dev/null)
      low=$(cat "$gpu/min_freq" 2>/dev/null)
      case "$low:$max" in *[!0-9:]*|'') continue;;esac
      # Common GPU devfreq uses Hz; some kernels export kHz.
      if [ "$max" -ge 10000000 ];then
        freq=$((gpumhz*1000000))
      else
        freq=$((gpumhz*1000))
      fi
      safe_floor "$gpu/min_freq" "$freq" "$low" "$max" "$gpu/available_frequencies"
      break
    done
  fi
}
case "${1:-}" in
  restore) restore;;
  apply)
    [ $# -eq 4 ] || exit 64
    apply "$2" "$3" "$4";;
  *) echo 'Usage: perf-guard.sh restore | apply PKG CPU_KHZ GPU_MHZ' >&2;exit 64;;
esac
