#!/bin/sh
# Locate the extension from this script, so the package does not have to be
# unpacked into one hardcoded directory (KUAL uses /mnt/us/extensions/kterm,
# KPM installs into /mnt/us/kterm, ...).
SELF=$0
case "$SELF" in
  */*) ;;
  *) SELF=$(command -v "$SELF" 2>/dev/null || echo "$SELF") ;;
esac
EXTENSION=$(cd "$(dirname "$SELF")/.." 2>/dev/null && pwd)
if [ -z "$EXTENSION" ]; then
  EXTENSION=/mnt/us/extensions/kterm
fi

DPI=`cat /var/log/Xorg.0.log 2>/dev/null | grep DPI | sed -n 's/.*(\([0-9]\+\), [0-9]\+).*/\1/p' | head -n 1`
#use different layouts for high resolution devices
PARAM=""
if [ -n "$DPI" ]; then
  if [ "$DPI" -gt 290 ] && [ -r "${EXTENSION}/layouts/keyboard-300dpi.xml" ]; then
    PARAM="-l ${EXTENSION}/layouts/keyboard-300dpi.xml"
  elif [ "$DPI" -gt 200 ] && [ -r "${EXTENSION}/layouts/keyboard-200dpi.xml" ]; then
    PARAM="-l ${EXTENSION}/layouts/keyboard-200dpi.xml"
  fi
fi
# always pass a layout that exists; kterm also falls back on its own if this
# one is missing
if [ -z "$PARAM" ] && [ -r "${EXTENSION}/layouts/keyboard.xml" ]; then
  PARAM="-l ${EXTENSION}/layouts/keyboard.xml"
fi

export TERM=xterm TERMINFO=${EXTENSION}/vte/terminfo
exec ${EXTENSION}/bin/kterm ${PARAM} "$@"
