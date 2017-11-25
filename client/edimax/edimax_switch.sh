#!/bin/bash

# Read config:
source edimax_config.txt || exit

# Get switch state argument:
TOSTATE=$1
if [[ "${TOSTATE}" = "" ]] ; then
	echo "Usage: $0 [ON|OFF]"
	exit
fi

# Switch Edimax and read result:
RESULT=$( ( curl --silent --data @- --${EDIMAX_AUTH:-basic} --user ${EDIMAX_USER}:${EDIMAX_PASS} http://${EDIMAX_IP}:10000/smartplug.cgi --output - <<-__XML__
	<?xml version="1.0" encoding="utf-8"?>
	<SMARTPLUG id="edimax"><CMD id="setup"><Device.System.Power.State>${TOSTATE}</Device.System.Power.State></CMD></SMARTPLUG>
__XML__
) | sed -e 's:.*<CMD id="setup">\([^<]*\).*:\1:g' )

# Output result:
echo ${RESULT}
