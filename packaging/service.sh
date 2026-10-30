#!/system/bin/sh
# Persistent game detection. No performance adjustment unless a user profile
# explicitly configures CPU or GPU frequency floors.
ROOT=${0%/*}
CLI="$ROOT/bin/fps-profilectl"
GUARD="$ROOT/perf-guard.sh"
[ -x "$CLI" ] && [ -f "$GUARD" ] || exit 0
# Restore stale previous state if the service was interrupted.
sh "$GUARD" restore
LAST=""
while true;do
  LINES=$("$CLI" runtime 2>/dev/null)
  PICK=$(printf '%s\n' "$LINES" | while read -r pkg cpu gpu;do
    case "$pkg" in ''|*[!a-zA-Z0-9._]*)continue;;esac
    if pidof "$pkg" >/dev/null 2>&1;then
      printf '%s %s %s\n' "$pkg" "$cpu" "$gpu"
      break
    fi
  done)
  if [ "$PICK" != "$LAST" ];then
    if [ -z "$PICK" ];then
      sh "$GUARD" restore
    else
      set -- $PICK
      sh "$GUARD" apply "$1" "$2" "$3"
    fi
    LAST=$PICK
  fi
  sleep 4
done
