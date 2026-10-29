#!/system/bin/sh
# Minimal module setup: never alter OEM services, GPU drivers, scheduler or display properties.
MODDIR=${0%/*}
CONFIG=/data/local/tmp/TargetList.json

# Do not overwrite existing user configuration.
if [ ! -e "$CONFIG" ] && [ -f "$MODDIR/TargetList.json" ]; then
  cp "$MODDIR/TargetList.json" "$CONFIG" || exit 0
  chmod 0600 "$CONFIG"
fi
