#!/system/bin/sh
# OPGCE v3.1.6 interoperability layer: strictly read-only.
# Never invoke OPGCE inject_core/action/uninstall, alter COSA DB or touch mode.
OPGCE_DIR=${OPGCE_DIR:-/data/adb/modules/cosa_injector}
MOD_PROP="$OPGCE_DIR/module.prop"
STRATEGIES="$OPGCE_DIR/encrypted_strategy"
ALIASES="$OPGCE_DIR/device_config.json"
installed() { [ -f "$MOD_PROP" ]; }
enabled() { installed && [ ! -e "$OPGCE_DIR/disable" ] && [ ! -e "$OPGCE_DIR/remove" ]; }
valid_package() {
  case "$1" in
    ''|.*|*..*|*[!a-zA-Z0-9._]*|*[!.a-zA-Z0-9_]) return 1;;
  esac
  case "$1" in *.*) return 0;; *) return 1;; esac
}
# Active strategy files were copied to the root of encrypted_strategy/
# during OPGCE installation. Directory names alone don't prove activation.
profile_for() {
  pkg=$1
  valid_package "$pkg" || return 1
  enabled || return 1
  [ -d "$STRATEGIES" ] || return 1
  [ -f "$STRATEGIES/$pkg.enc" ] && return 0
  [ -r "$ALIASES" ] || return 1
  # OPGCE device_config.json stores each canonical game's aliases on one
  # line in v3.1.6. Match quoted tokens exactly, not substrings of package.
  for f in "$STRATEGIES"/*.enc;do
    [ -f "$f" ] || continue
    base=${f##*/}
    base=${base%.enc}
    grep -F "\"$base\"" "$ALIASES" 2>/dev/null |
      grep -Fq "\"$pkg\"" && return 0
  done
  return 1
}
case "${1:-}" in
  status)
    if installed;then
      echo 'INSTALLED|1'
      if enabled;then echo 'ENABLED|1';else echo 'ENABLED|0';fi
      version=$(sed -n 's/^version=//p' "$MOD_PROP" | head -n 1)
      printf 'VERSION|%s\n' "$version"
    else
      echo 'INSTALLED|0'
      echo 'ENABLED|0'
      echo 'VERSION|unavailable'
    fi
    if [ -f /data/data/com.oplus.cosa/databases/db_game_database ] ||
       [ -f /data/user_de/0/com.oplus.cosa/databases/db_game_database ] ||
       [ -f /data_ce/data/com.oplus.cosa/databases/db_game_database ];then
      echo 'COSA_DB|1'
    else
      echo 'COSA_DB|0'
    fi
    if enabled && [ -r /dev/opgce/mode ] && [ ! -L /dev/opgce/mode ];then
      mode=$(cat /dev/opgce/mode 2>/dev/null)
      case "$mode" in fast|powersave|balance|performance)
        printf 'MODE|%s\n' "$mode";;
      *) echo 'MODE|unknown';;
      esac
    else
      echo 'MODE|unavailable'
    fi
    count=0
    if enabled;then
      for f in "$STRATEGIES"/*.enc;do
        [ -f "$f" ] && count=$((count+1))
      done
    fi
    printf 'ACTIVE_PROFILES|%s\n' "$count"
    # Mode is a powercfg state marker, not proof of active OS scheduling.
    ;;
  managed)
    [ "$#" -eq 2 ] && profile_for "$2"
    ;;
  list)
    enabled || exit 0
    for f in "$STRATEGIES"/*.enc;do
      [ -f "$f" ] || continue
      pkg=${f##*/};pkg=${pkg%.enc}
      valid_package "$pkg" || continue
      printf 'OPGCE|%s\n' "$pkg"
      [ -r "$ALIASES" ] || continue
      # Aliases are metadata, not guaranteed injected policies. Output only
      # tokens shaped like Android package identifiers.
      line=$(grep -F "\"$pkg\"" "$ALIASES" 2>/dev/null | head -n 1)
      [ -n "$line" ] || continue
      printf '%s\n' "$line" | grep -oE '[a-zA-Z_][a-zA-Z0-9_]*(\.[a-zA-Z0-9_]+)+' |
        while IFS= read -r alias;do
          valid_package "$alias" || continue
          [ "$alias" = "$pkg" ] || printf 'OPGCE_ALIAS|%s\n' "$alias"
        done
    done
    ;;
  *)
    echo 'usage: opgce-bridge.sh status|list|managed PACKAGE' >&2
    exit 64;;
esac
