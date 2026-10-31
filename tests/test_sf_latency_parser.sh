#!/bin/sh
set -eu
SCRIPT=packaging/sf-present-fps.sh
PARSER=packaging/sf_latency_parser.awk
sh -n "$SCRIPT"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cat > "$WORK/60hz.txt" <<'EOF'
16666667
1999999999999 2000000000000 2000000200000
2000016666666 2000016666667 2000016866667
2000033333333 2000033333334 2000033533334
2000050000000 2000050000001 2000050200001
2000050000000 2000050000001 2000050200001
2000050000000 9223372036854775807 2000050200001
EOF
sh "$SCRIPT" --file "$WORK/60hz.txt" > "$WORK/60out.txt"
grep -qx 'surface_present_fps=60.000' "$WORK/60out.txt"
grep -qx 'present_samples=4' "$WORK/60out.txt"
grep -qx 'discarded_samples=2' "$WORK/60out.txt"
grep -qx 'frame_interval_p50_ms=16.667' "$WORK/60out.txt"
cat > "$WORK/120hz.txt" <<'EOF'
8333333
1999999999999 2000000000000 2000000200000
2000008333332 2000008333333 2000008533333
2000016666665 2000016666666 2000016866666
EOF
awk -f "$PARSER" "$WORK/120hz.txt" > "$WORK/120out.txt"
grep -qx 'surface_present_fps=120.000' "$WORK/120out.txt"
cat > "$WORK/empty.txt" <<'EOF'
16666667
0 0 0
1 9223372036854775807 1
EOF
if awk -f "$PARSER" "$WORK/empty.txt" > "$WORK/emptyout.txt"; then
  echo 'FAIL: empty latency incorrectly reported measurable FPS' >&2;exit 1
fi
grep -qx 'surface_present_fps=unavailable' "$WORK/emptyout.txt"
echo 'SurfaceFlinger read-only FPS parser: 60/120/no-samples passed'
