#!/system/bin/sh
# Sourced by the KernelSU / Magisk / APatch module installer after extraction.
# Do not set SKIPUNZIP: the manager must unpack the module normally.
#
# KernelSU normally resets extracted files to 0644. ZIP executable mode is
# NOT sufficient for binaries that will be executed from module WebUI.
# Missing this step caused "ERROR: profile helper missing" on v0.7.0.
[ -n "${MODPATH:-}" ] || {
  echo "! MODPATH unavailable: cannot set module executable permissions"
  return 1 2>/dev/null || exit 1
}
apply_exec_perm() {
  f="$MODPATH/$1"
  [ -f "$f" ] && [ ! -L "$f" ] || {
    echo "! Module is incomplete: $1 is missing"
    return 1
  }
  if command -v set_perm >/dev/null 2>&1; then
    set_perm "$f" 0 0 0755 || return 1
  else
    # Conservative fallback for nonstandard compatible installers.
    chown 0:0 "$f" 2>/dev/null || true
    chmod 0755 "$f" || return 1
  fi
  [ -x "$f" ] || { echo "! Failed to mark $1 executable";return 1; }
}
for f in \
  bin/fps-profilectl \
  bin/profile-ui.sh \
  service.sh \
  post-fs-data.sh \
  perf-guard.sh \
  opgce-bridge.sh \
  scrc-bridge.sh \
  sf-present-fps.sh \
  fps-evidence.sh \
  fengchi-probe.sh;do
  apply_exec_perm "$f" || {
    echo "! Aborting installation: mandatory runtime file failed validation"
    return 1 2>/dev/null || exit 1
  }
done
[ -s "$MODPATH/zygisk/arm64-v8a.so" ] || {
  echo "! ARM64 Zygisk library missing"
  return 1 2>/dev/null || exit 1
}
[ -s "$MODPATH/webroot/index.html" ] &&
[ -s "$MODPATH/webroot/app.js" ] || {
  echo "! WebUI assets missing"
  return 1 2>/dev/null || exit 1
}
echo "- UnityFPSUnlocker: native executable permissions verified"
