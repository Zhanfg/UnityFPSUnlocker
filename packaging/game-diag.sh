#!/system/bin/sh
# Read-only per-game Android/Zygisk diagnostic. Never changes game or OEM nodes.
ROOT=${0%/*}
APP=${1:-}
case "$APP" in ''|.*|*..*|*[!a-zA-Z0-9._]*) echo 'ERROR|invalid-package';exit 64;;esac
case "$APP" in *.*) ;; *) echo 'ERROR|invalid-package';exit 64;;esac
printf 'PACKAGE|%s\n' "$APP"
printf 'ROOT_UID|%s\n' "$(id -u 2>/dev/null)"
if cmd package path "$APP" >/dev/null 2>&1 || pm path "$APP" >/dev/null 2>&1;then
  echo 'INSTALLED|1'
else
  echo 'INSTALLED|unknown'
fi
PIDS=$(pidof "$APP" 2>/dev/null || true)
if [ -z "$PIDS" ];then
  echo 'PROCESS|not-running'
  echo 'IL2CPP|unknown'
else
  echo "PROCESS|$PIDS"
  FOUND=0;ACCESS=0
  for p in $PIDS;do
    case "$p" in ''|*[!0-9]*) continue;;esac
    if [ -r "/proc/$p/maps" ];then
      ACCESS=1
      if grep -Fq '/libil2cpp.so' "/proc/$p/maps" 2>/dev/null;then FOUND=1;break;fi
    fi
  done
  if [ "$FOUND" = 1 ];then
    echo 'IL2CPP|loaded'
  elif [ "$ACCESS" = 1 ];then
    echo 'IL2CPP|not-observed'
  else
    echo 'IL2CPP|unreadable'
  fi
fi
if [ -r "$ROOT/zygisk/arm64-v8a.so" ];then echo 'ZYGISK_FILE|present';else echo 'ZYGISK_FILE|missing';fi
if [ -e "$ROOT/disable" ];then echo 'MODULE|disabled';else echo 'MODULE|enabled-marker';fi
# Collect only our own tag. Do not leak other application logs.
# Hook logs cannot be conclusively associated with an app unless their PID
# matches; logs from the zygisk companion may have a different PID.
if command -v logcat >/dev/null 2>&1;then
  logcat -d -t 500 -v threadtime -s UnityFPSUnlocker:D UnityFPSUnlocker:E 2>/dev/null |
    tail -n 18 | while IFS= read -r line;do
      printf 'RECENT_MODULE_LOG|%s\n' "$line"
    done
fi
