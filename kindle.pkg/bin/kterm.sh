#!/bin/sh
EXTENSION=/mnt/us/extensions/kterm
DPI=`cat /var/log/Xorg.0.log | grep DPI | sed -n 's/.*(\([0-9]\+\), [0-9]\+).*/\1/p'`
#use different layouts for high resolution devices
if [ ${DPI} -gt 290 ]; then
  PARAM="-l ${EXTENSION}/layouts/keyboard-300dpi.xml"
elif [ ${DPI} -gt 200 ]; then
  PARAM="-l ${EXTENSION}/layouts/keyboard-200dpi.xml"
fi
export TERM=xterm TERMINFO=${EXTENSION}/vte/terminfo
${EXTENSION}/bin/kterm ${PARAM} "$@"
