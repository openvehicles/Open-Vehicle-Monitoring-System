#!/bin/bash

NAME=$1

if test "${NAME}" == "" ; then
	echo "Usage: $0 basename"
	echo "e.g. $0 140118-test"
	exit
fi

echo "Fetch RT-ENG-LogKeyTime..."
./cmd.pl 32 "RT-ENG-LogKeyTime" > "${NAME}-LogKeyTime.csv"

echo "Fetch RT-ENG-LogAlerts..."
./cmd.pl 32 "RT-ENG-LogAlerts" > "${NAME}-LogAlerts.csv"

echo "Fetch RT-ENG-LogFaults..."
./cmd.pl 32 "RT-ENG-LogFaults" > "${NAME}-LogFaults.csv"

echo "Fetch RT-ENG-LogSystem..."
./cmd.pl 32 "RT-ENG-LogSystem" > "${NAME}-LogSystem.csv"

echo "Fetch RT-ENG-LogCounts..."
./cmd.pl 32 "RT-ENG-LogCounts" > "${NAME}-LogCounts.csv"

echo "Fetch RT-ENG-LogMinMax..."
./cmd.pl 32 "RT-ENG-LogMinMax" > "${NAME}-LogMinMax.csv"

echo "Done."

ls -l ${NAME}-*
