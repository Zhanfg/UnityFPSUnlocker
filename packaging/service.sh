#!/system/bin/sh
# Per-game frequency guard. OPGCE owns OEM COSA scheduling where its
# active encrypted-strategy root contains the game's canonical/alias profile.
# Unity native FPS interception remains independent of OEM scheduling.
ROOT=${0%/*}
CLI="$ROOT/bin/fps-profilectl"
GUARD="$ROOT/perf-guard.sh"
OPGCE="$ROOT/opgce-bridge.sh"
SCRC="$ROOT/scrc-bridge.sh"
[ -x "$CLI" ] && [ -f "$GUARD" ] || exit 0
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
  if [ -n "$PICK" ];then
    set -- $PICK
    # SCRC's upstream expressly discourages third-party scheduling modules.
    # Defer our CPU/GPU floor writes for *all* games whenever enabled,
    # even if no deployed SCRC profile matches a particular package.
    if [ -f "$SCRC" ] && sh "$SCRC" active >/dev/null 2>&1;then
      PICK="SCRC $1"
    # OPGCE installed != enabled; only deployed profiles count.
    elif [ -f "$OPGCE" ] && sh "$OPGCE" managed "$1" >/dev/null 2>&1;then
      PICK="OPGCE $1"
    else
      PICK="LOCAL $1 $2 $3"
    fi
  fi
  if [ "$PICK" != "$LAST" ];then
    case "$PICK" in
      "")
        sh "$GUARD" restore;;
      SCRC\ *)
        sh "$GUARD" restore
        log -t UnityFPSUnlocker "SCRC active: suppressing local CPU/GPU floor writes" 2>/dev/null || true;;
      OPGCE\ *)
        # An explicitly managed COSA profile gets sole ownership of device
        # scheduling. No CPU/GPU floor writes from UnityFPSUnlocker.
        sh "$GUARD" restore
        log -t UnityFPSUnlocker "OPGCE profile selected, local CPU/GPU floors suppressed: ${PICK#OPGCE }" 2>/dev/null || true;;
      LOCAL\ *)
        set -- $PICK
        sh "$GUARD" apply "$2" "$3" "$4";;
    esac
    LAST=$PICK
  fi
  sleep 4
done
