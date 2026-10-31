#!/bin/sh
set -eu
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
mkdir -p "$WORK/module/bin" "$WORK/fakebin"
cp packaging/profile-ui.sh "$WORK/module/bin/profile-ui.sh"
cat > "$WORK/fakebin/pm" <<'EOF'
#!/bin/sh
[ "$1" = list ] && [ "$2" = packages ] && echo package:com.example.game
EOF
chmod 755 "$WORK/fakebin/pm"
# No native binary: list and scan MUST remain usable, although profile IO
# must clearly fail. This was previously broken by a global -x guard.
PATH="$WORK/fakebin:$PATH" sh "$WORK/module/bin/profile-ui.sh" scan > "$WORK/scan"
grep -qx 'GAME|com.example.game' "$WORK/scan"
if sh "$WORK/module/bin/profile-ui.sh" read > "$WORK/read" 2>"$WORK/error";then
  echo 'Missing native helper accepted' >&2;exit 1
fi
grep -q 'profile helper file not installed:' "$WORK/error"
# Old installation scenario: binary was unpacked 0644. The bridge should
# safely repair its own helper executable bit before profile reads.
cat > "$WORK/module/bin/fps-profilectl" <<'EOF'
#!/bin/sh
[ "$1" = read ] && echo '{"custom":{}}'
EOF
chmod 644 "$WORK/module/bin/fps-profilectl"
sh "$WORK/module/bin/profile-ui.sh" read > "$WORK/recovered"
grep -qx '{"custom":{}}' "$WORK/recovered"
[ -x "$WORK/module/bin/fps-profilectl" ]
echo 'Native helper fallback, scan isolation and 0644 repair PASS'
