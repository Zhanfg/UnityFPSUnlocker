#!/system/bin/sh
# SCRC v5.7 compatibility metadata ONLY.
# No COSA database changes, inject binaries, kernel insmod, service restart or cleanup.
SCRC_DIR=${SCRC_DIR:-/data/adb/modules/scrc}
PROP="$SCRC_DIR/module.prop"
CONFIG="$SCRC_DIR/encrypted_oplus-config"
OPGCE_DIR=${OPGCE_DIR:-/data/adb/modules/cosa_injector}
is_installed() { [ -f "$PROP" ]; }
is_enabled() { is_installed && [ ! -e "$SCRC_DIR/disable" ] && [ ! -e "$SCRC_DIR/remove" ]; }
is_opgce_enabled() {
  [ -f "$OPGCE_DIR/module.prop" ] &&
  [ ! -e "$OPGCE_DIR/disable" ] && [ ! -e "$OPGCE_DIR/remove" ]
}
valid_package() {
  case "$1" in ''|.*|*..*|*[!a-zA-Z0-9._]*) return 1;; esac
  case "$1" in *.*) return 0;; esac
  return 1
}
case "${1:-}" in
 status)
   if is_installed;then
      echo 'INSTALLED|1'
      is_enabled && echo 'ENABLED|1' || echo 'ENABLED|0'
      v=$(sed -n 's/^version=//p' "$PROP" | head -n1)
      printf 'VERSION|%s\n' "$v"
   else
      echo 'INSTALLED|0'; echo 'ENABLED|0'; echo 'VERSION|unavailable'
   fi
   n=0
   if is_enabled;then
     for f in "$CONFIG"/*.enc "$CONFIG"/*.json;do
       [ -f "$f" ] && n=$((n+1))
     done
   fi
   printf 'DEPLOYED_PROFILES|%s\n' "$n"
   # This is *evidence of another installed injector*, not proof of an
   # actual active collision in COSA. Flag it as an integration warning.
   is_enabled && is_opgce_enabled && echo 'DUAL_INJECTORS|1' || echo 'DUAL_INJECTORS|0'
   if [ -e /proc/game_opt/task_boost/critical_task_name ];then
      echo 'OPGS_NODE|1'
   else
      echo 'OPGS_NODE|0'
   fi
   # Supplemental .ko is hardcoded to one 6.1.141 build in supplied ZIP.
   # Do not try to load it or claim 6.6-compatible support.
   case "$(uname -r)" in
    6.1.141-android14-11-o-*) echo 'OPGS_KERNEL_FAMILY|6.1.141-candidate';;
    *) echo 'OPGS_KERNEL_FAMILY|unsupported-for-bundled-ko';;
   esac
   ;;
 list)
   is_enabled || exit 0
   for f in "$CONFIG"/*.enc "$CONFIG"/*.json;do
      [ -f "$f" ] || continue
      p=${f##*/};p=${p%.enc};p=${p%.json}
      valid_package "$p" || continue
      printf 'SCRC|%s\n' "$p"
   done
   ;;
 managed)
   [ "$#" -eq 2 ] || exit 64
   valid_package "$2" || exit 1
   is_enabled || exit 1
   [ -f "$CONFIG/$2.enc" ] || [ -f "$CONFIG/$2.json" ]
   ;;
 active)
   is_enabled
   ;;
 *)
   echo 'usage: scrc-bridge.sh status|list|managed PACKAGE|active' >&2
   exit 64;;
esac
