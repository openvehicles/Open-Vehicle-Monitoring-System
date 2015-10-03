#!/bin/bash

# Read config:
source edimax_config.txt || exit

# Read current SOC:
SOC=$(./status.pl | sed 's:MP-0 S\([0-9]*\).*:\1:g')
if [[ "$SOC" = "" ]] ; then
	logger -p user.error "${0}: SOC level unknown, aborting"
	echo "SOC level unknown, aborting" >&2
	exit
fi

# Read current Edimax state:
STATE=$(./edimax_get.sh)
if [[ "$STATE" = "" ]] ; then
	logger -p user.error "${0}: Plug state unknown, aborting"
	echo "Plug state unknown, aborting" >&2
	exit
fi

# Switch Edimax Smart Plug according to SOC level:
if [[ $SOC -ge $SOC_OFF && $STATE = "ON" ]] ; then
	logger "${0}: SOC level ${SOC}%, switching OFF"
	echo -n "SOC level ${SOC}%, switching OFF... "
	./edimax_switch.sh OFF
elif [[ $SOC -le $SOC_ON && $STATE = "OFF" ]] ; then
	logger "${0}: SOC level ${SOC}%, switching ON"
	echo -n "SOC level ${SOC}%, switching ON... "
	./edimax_switch.sh ON
fi
