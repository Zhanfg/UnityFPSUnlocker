#!/system/bin/sh
# Read-only Android SurfaceFlinger presentation-rate diagnostic.
set -u
SELF_DIR=${0%/*}
PARSER="$SELF_DIR/sf_latency_parser.awk"
case ${1:-} in
  --file)
    [ $# = 2 ] && [ -r "$2" ] || { echo "usage: sh sf-present-fps.sh --file latency-dump.txt" >&2; exit 64; }
    exec awk -f "$PARSER" "$2"
    ;;
  --package)
    [ $# = 2 ] || { echo "usage: sh sf-present-fps.sh --package com.example.game" >&2;exit 64;}
    PACKAGE=$2
    case "$PACKAGE" in ''|*[!a-zA-Z0-9._]*) echo "ERROR invalid package" >&2;exit 64;; esac
    command -v dumpsys >/dev/null 2>&1 || { echo "ERROR dumpsys unavailable" >&2;exit 72;}
    LAYERS=$(dumpsys SurfaceFlinger --list 2>/dev/null | grep -F "$PACKAGE" | grep 'SurfaceView' || true)
    COUNT=$(printf '%s\n' "$LAYERS" | grep -c '.' || true)
    if [ "$COUNT" != 1 ]; then
      echo "ERROR expected exactly one SurfaceView for package; found=$COUNT" >&2
      echo "Candidates (do not guess the layer):" >&2
      dumpsys SurfaceFlinger --list 2>/dev/null | grep -F "$PACKAGE" >&2 || true
      echo 'Use --layer "exact SurfaceFlinger layer name"' >&2
      exit 73
    fi
    LAYER=$LAYERS
    ;;
  --layer)
    [ $# = 2 ] && [ -n "$2" ] || { echo "usage: sh sf-present-fps.sh --layer 'exact layer name'" >&2;exit 64;}
    LAYER=$2
    ;;
  *)
    echo 'usage: sh sf-present-fps.sh --package com.example.game | --layer "exact name" | --file path' >&2
    exit 64 ;;
esac
[ -r "$PARSER" ] || { echo "ERROR missing parser: $PARSER" >&2;exit 70;}
TMPBASE=/data/local/tmp
[ -d "$TMPBASE" ] && [ -w "$TMPBASE" ] || TMPBASE=/sdcard/Download
TMP="$TMPBASE/sf_latency_$$.txt"
trap 'rm -f "$TMP"' 0 1 2 3 15
if ! dumpsys SurfaceFlinger --latency "$LAYER" > "$TMP" 2>/dev/null;then
  echo "ERROR SurfaceFlinger latency unavailable on this build" >&2;exit 72
fi
echo "layer_name=$LAYER"
awk -f "$PARSER" "$TMP"
