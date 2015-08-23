#!/bin/bash

# Read config:
source edimax_config.txt || exit

# Read current SOC:
SOC=$(./status.pl | sed 's:MP-0 S\([0-9]*\).*:\1:g')
if [[ "$SOC" = "" ]] ; then
	echo "SOC level unknown, aborting"
	exit
fi

# Read current Edimax state:
STATE=$(./edimax_get.sh)
if [[ "$STATE" = "" ]] ; then
	echo "Plug state unknown, aborting"
	exit
fi

# Switch Edimax Smart Plug according to SOC level:
if [[ $SOC -ge $SOC_OFF && $STATE = "ON" ]] ; then
	echo -n "SOC level ${SOC}%, switching OFF... "
	./edimax_switch.sh OFF
elif [[ $SOC -le $SOC_ON && $STATE = "OFF" ]] ; then
	echo -n "SOC level ${SOC}%, switching ON... "
	./edimax_switch.sh ON
fi
