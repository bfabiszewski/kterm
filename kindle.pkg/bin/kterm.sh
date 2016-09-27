#!/bin/sh
EXTENSION=/mnt/us/extensions/kterm
#WIDTH=`xwininfo -root|grep Width|cut -d ':' -f 2`
# use wider keyboard layout for screen width greater than 600 px
#if [ ${WIDTH} -gt 600 ]; then
#  PARAM="-l ${EXTENSION}/layouts/keyboard-wide.xml"
#fi
${EXTENSION}/bin/kterm ${PARAM}
