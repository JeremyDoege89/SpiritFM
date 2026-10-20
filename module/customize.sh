# Magisk/KernelSU installer hook: the APK and its libs are already laid out under system/priv-app/SpiritFM.
ui_print "- Spirit FM will run as a privileged app after reboot"
ui_print "- Grant it root in your root manager when asked"
set_perm_recursive "$MODPATH/system" 0 0 0755 0644
