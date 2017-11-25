#!/bin/bash

# Read config:
source edimax_config.txt || exit

# Get Edimax state:
RESULT=$( ( curl --silent --data @- --${EDIMAX_AUTH:-basic} --user ${EDIMAX_USER}:${EDIMAX_PASS} http://${EDIMAX_IP}:10000/smartplug.cgi --output - <<-__XML__
	<?xml version="1.0" encoding="utf-8"?>
	<SMARTPLUG id="edimax"><CMD id="get"><Device.System.Power.State></Device.System.Power.State></CMD></SMARTPLUG>
__XML__
) | sed -e 's:.*<Device.System.Power.State>\([^<]*\).*:\1:g' )

# Output result:
echo -n ${RESULT}
