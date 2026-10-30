#!/usr/bin/env bash
set -euo pipefail
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
CXX="${CXX:-c++}"
"$CXX" -std=c++17 -O1 -IUnityFPSUnlocker/third/rapidjson \
  -DFPS_PROFILE_CONFIG_PATH="\"$WORK/TargetList.json\"" \
  tools/fps-profilectl.cpp -o "$WORK/fps-profilectl"
cat > "$WORK/TargetList.json" <<'EOF'
{"global":{"fps":0,"delay":0,"mod_opcode":false,"render_interval":0,"scale":-1},"custom":{"com.example.demo":{"fps":60,"custom_field":"keep"}}}
EOF
"$WORK/fps-profilectl" set com.example.demo 144 1 1800000 650 0 | grep -qx OK
"$WORK/fps-profilectl" set com.example.another 0 0 0 0 4 | grep -qx OK
"$WORK/fps-profilectl" runtime > "$WORK/runtime.txt"
grep -qx 'com.example.demo 1800000 650' "$WORK/runtime.txt"
python3 - "$WORK/TargetList.json" <<'PY'
import json,sys
doc=json.load(open(sys.argv[1]))
assert doc["global"]["fps"]==0
assert doc["custom"]["com.example.demo"]["custom_field"]=="keep"
assert doc["custom"]["com.example.demo"]["fps"]==144
assert doc["custom"]["com.example.demo"]["render_interval"]==1
assert doc["custom"]["com.example.demo"]["cpu_min_khz"]==1800000
assert doc["custom"]["com.example.demo"]["gpu_min_mhz"]==650
assert doc["custom"]["com.example.another"]["fps"]==0
PY
before=$(sha256sum "$WORK/TargetList.json")
if "$WORK/fps-profilectl" set 'com.demo;id' 144 0 0 0 0;then echo 'injection accepted' >&2;exit 1;fi
if "$WORK/fps-profilectl" set com.bad.demo 999 0 0 0 0;then echo 'invalid FPS accepted' >&2;exit 1;fi
if "$WORK/fps-profilectl" set com.bad.demo 60 0 0 8000 0;then echo 'invalid GPU accepted' >&2;exit 1;fi
after=$(sha256sum "$WORK/TargetList.json")
[ "$before" = "$after" ] || { echo 'rejected writes mutated configuration' >&2;exit 1; }
"$WORK/fps-profilectl" remove com.example.demo | grep -qx OK
python3 - "$WORK/TargetList.json" <<'PY'
import json,sys
doc=json.load(open(sys.argv[1]))
assert "com.example.demo" not in doc["custom"]
assert "com.example.another" in doc["custom"]
PY
printf 'Invalid JSON' > "$WORK/TargetList.json"
if "$WORK/fps-profilectl" set com.example.demo 60 0 0 0 0;then echo 'corrupted JSON overwritten' >&2;exit 1;fi
grep -qx 'Invalid JSON' "$WORK/TargetList.json"
echo 'Profile persistence, isolation, input rejection, preservation and corrupted-config safety PASS'
