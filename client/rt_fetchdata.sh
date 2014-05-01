#!/bin/bash

NAME=$1

if test "${NAME}" == "" ; then
	echo "Usage: $0 basename"
	echo "e.g. $0 130311-tour1"
	exit
fi

echo "Fetch *-OVM-DebugCrash..."
./cmd.pl 32 "*-OVM-DebugCrash" > "${NAME}-debugcrash.csv"

echo "Fetch RT-PWR-BattCell..."
./cmd.pl 32 "RT-PWR-BattCell" > "${NAME}-battcell.csv"

echo "Fetch RT-PWR-BattPack..."
./cmd.pl 32 "RT-PWR-BattPack" > "${NAME}-battpack.csv"

echo "Fetch RT-PWR-UsageStats..."
./cmd.pl 32 "RT-PWR-UsageStats" > "${NAME}-usage.csv"

echo "Fetch RT-GPS-Log..."
./cmd.pl 32 "RT-GPS-Log" > "${NAME}-gpslog.csv"

echo "Fetch RT-PWR-Log..."
./cmd.pl 32 "RT-PWR-Log" > "${NAME}-pwrlog.csv"

echo "Done."

ls -l ${NAME}-*
