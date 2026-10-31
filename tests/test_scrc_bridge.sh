#!/bin/sh
set -eu
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
SCRC="$WORK/modules/scrc"
OPGCE="$WORK/modules/cosa_injector"
mkdir -p "$SCRC/encrypted_oplus-config" "$OPGCE"
printf 'id=scrc\nversion=v5.7\n' > "$SCRC/module.prop"
printf 'id=cosa_injector\n' > "$OPGCE/module.prop"
printf 'opaque\n' > "$SCRC/encrypted_oplus-config/com.miHoYo.Yuanshen.enc"
printf '{"fps":60}\n' > "$SCRC/encrypted_oplus-config/com.netease.sample.json"
touch "$SCRC/encrypted_oplus-config/invalid;pkg.enc"
cat > "$SCRC/bin-inject" <<EOF
#!/bin/sh
touch "$WORK/EXECUTED"
EOF
chmod +x "$SCRC/bin-inject"
B=packaging/scrc-bridge.sh
sh -n "$B"
SCRC_DIR="$SCRC" OPGCE_DIR="$OPGCE" sh "$B" status > "$WORK/status"
grep -qx 'INSTALLED|1' "$WORK/status"
grep -qx 'ENABLED|1' "$WORK/status"
grep -qx 'VERSION|v5.7' "$WORK/status"
grep -qx 'DUAL_INJECTORS|1' "$WORK/status"
grep -qx 'DEPLOYED_PROFILES|3' "$WORK/status"
SCRC_DIR="$SCRC" OPGCE_DIR="$OPGCE" sh "$B" list > "$WORK/list"
grep -qx 'SCRC|com.miHoYo.Yuanshen' "$WORK/list"
grep -qx 'SCRC|com.netease.sample' "$WORK/list"
! grep -q 'invalid' "$WORK/list"
SCRC_DIR="$SCRC" OPGCE_DIR="$OPGCE" sh "$B" managed com.miHoYo.Yuanshen
if SCRC_DIR="$SCRC" OPGCE_DIR="$OPGCE" sh "$B" managed com.unknown.app;then
  echo 'SCRC bridge falsely matched an undeployed game' >&2;exit 1
fi
touch "$SCRC/disable"
if SCRC_DIR="$SCRC" OPGCE_DIR="$OPGCE" sh "$B" active;then
  echo 'Disabled SCRC counted active' >&2;exit 1
fi
SCRC_DIR="$SCRC" OPGCE_DIR="$OPGCE" sh "$B" status > "$WORK/disabled"
grep -qx 'ENABLED|0' "$WORK/disabled"
grep -qx 'DUAL_INJECTORS|0' "$WORK/disabled"
test ! -e "$WORK/EXECUTED"
echo "SCRC read-only coexistence tests PASS"
