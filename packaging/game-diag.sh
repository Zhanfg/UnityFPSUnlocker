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
# SukiSU Ultra manages root permissions but is not, by itself, proof that
# a Zygisk provider is active. Detect possible enabled provider modules
# without calling their binaries, altering mount namespaces or declaring
# injection success based on a module.prop file.
ZYGISK_CANDIDATES=""
for module in /data/adb/modules/*;do
  [ -f "$module/module.prop" ] || continue
  [ ! -e "$module/disable" ] && [ ! -e "$module/remove" ] || continue
  [ "$module" = "$ROOT" ] && continue
  provider_id=$(sed -n 's/^id=//p' "$module/module.prop" | head -n 1)
  case "$provider_id" in
    *zygisk*|*Zygisk*|*Zygisk*|*rezygisk*|*ReZygisk*)
      if [ -z "$ZYGISK_CANDIDATES" ];then
        ZYGISK_CANDIDATES="$provider_id"
      else
        ZYGISK_CANDIDATES="$ZYGISK_CANDIDATES,$provider_id"
      fi
      ;;
  esac
done
if [ -n "$ZYGISK_CANDIDATES" ];then
  printf 'ZYGISK_PROVIDER_CANDIDATES|%s\n' "$ZYGISK_CANDIDATES"
else
  echo 'ZYGISK_PROVIDER_CANDIDATES|none-detected'
fi
echo 'ZYGISK_INJECTION_CONFIRMED|not-verified'
# Collect only our own tag. Do not leak other application logs.
# Hook logs cannot be conclusively associated with an app unless their PID
# matches; logs from the zygisk companion may have a different PID.
if command -v logcat >/dev/null 2>&1;then
  logcat -d -t 500 -v threadtime -s UnityFPSUnlocker:D UnityFPSUnlocker:E 2>/dev/null |
    tail -n 18 | while IFS= read -r line;do
      printf 'RECENT_MODULE_LOG|%s\n' "$line"
    done
fi
