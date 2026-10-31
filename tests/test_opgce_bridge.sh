#!/bin/sh
set -eu
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
MOD="$WORK/cosa_injector"
mkdir -p "$MOD/encrypted_strategy"
cat > "$MOD/module.prop" <<'EOF'
id=cosa_injector
name=OP Gaming Core Enhancer
version=3.1.6
EOF
cat > "$MOD/device_config.json" <<'EOF'
{"games":{
 "com.miHoYo.Yuanshen":["com.miHoYo.GenshinImpact","com.miHoYo.ys.bilibili"],
 "com.kurogame.mingchao":["com.tencent.ngr","com.kurogame.mingchao.bilibili"]
}}
EOF
echo '{"cipher":"opaque"}' > "$MOD/encrypted_strategy/com.miHoYo.Yuanshen.enc"
echo '{"cipher":"opaque"}' > "$MOD/encrypted_strategy/com.kurogame.mingchao.enc"
# Deliberately mock an injector. Our bridge MUST never call it.
cat > "$MOD/inject_core" <<EOF
#!/bin/sh
touch "$WORK/inject_called"
EOF
chmod +x "$MOD/inject_core"
B=packaging/opgce-bridge.sh
sh -n "$B"
OPGCE_DIR="$MOD" sh "$B" status > "$WORK/status"
grep -qx 'INSTALLED|1' "$WORK/status"
grep -qx 'ENABLED|1' "$WORK/status"
grep -qx 'VERSION|3.1.6' "$WORK/status"
grep -qx 'ACTIVE_PROFILES|2' "$WORK/status"
OPGCE_DIR="$MOD" sh "$B" managed com.miHoYo.Yuanshen
OPGCE_DIR="$MOD" sh "$B" managed com.miHoYo.GenshinImpact
OPGCE_DIR="$MOD" sh "$B" managed com.tencent.ngr
if OPGCE_DIR="$MOD" sh "$B" managed com.example.nonexistent;then
  echo 'Unknown package falsely matched' >&2;exit 1
fi
OPGCE_DIR="$MOD" sh "$B" list > "$WORK/list"
grep -qx 'OPGCE|com.miHoYo.Yuanshen' "$WORK/list"
grep -qx 'OPGCE_ALIAS|com.miHoYo.GenshinImpact' "$WORK/list"
grep -qx 'OPGCE_ALIAS|com.tencent.ngr' "$WORK/list"
touch "$MOD/disable"
if OPGCE_DIR="$MOD" sh "$B" managed com.miHoYo.Yuanshen;then
  echo 'Disabled OPGCE falsely considered active' >&2;exit 1
fi
OPGCE_DIR="$MOD" sh "$B" status > "$WORK/disabled"
grep -qx 'ENABLED|0' "$WORK/disabled"
test ! -e "$WORK/inject_called"
# Assert no write-oriented OPGCE operation in bridge implementation.
if grep -Eq '(inject_core|pm clear|sqlite3|settings put|mount --bind)' "$B";then
  echo 'Bridge contains forbidden OPGCE mutation' >&2;exit 1
fi
echo 'OPGCE coexistence: installed/disabled/active strategies/aliases/read-only PASSED'
