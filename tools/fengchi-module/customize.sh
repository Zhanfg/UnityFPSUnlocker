# Called from the module manager installer.
if [ "$ARCH" != "arm64" ]; then
  abort "! Fengchi Direct Lab requires Android ARM64."
fi
ui_print "- Installing Fengchi Direct Lab (experimental)"
ui_print "- No boot service, overlay, GPU driver or system property changes"
ui_print "- No automatic frame-boost activation"
ui_print "- After installation: su -c 'sh /data/adb/modules/fengchi_direct_lab/bin/fengchi status'"
set_perm_recursive "$MODPATH/bin" 0 0 0755 0755
