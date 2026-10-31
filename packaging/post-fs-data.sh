#!/system/bin/sh
# Persistent, root-owned, non-module storage: survives module upgrades.
# Migrate existing FPS profiles without overwriting user data.
MODDIR=${0%/*}
STORE=/data/adb/ufps-webui
CONFIG="$STORE/TargetList.json"
LEGACY=/data/local/tmp/TargetList.json

mkdir -p "$STORE" || exit 1
chmod 0700 "$STORE" || exit 1
if [ ! -e "$CONFIG" ];then
  if [ -f "$LEGACY" ] && [ ! -L "$LEGACY" ];then
    cp "$LEGACY" "$CONFIG" || exit 1
  else
    cp "$MODDIR/TargetList.json" "$CONFIG" || exit 1
  fi
fi
chmod 0600 "$CONFIG"
