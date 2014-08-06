#!/bin/bash

NAME=$1

if test "${NAME}" == "" ; then
	echo "Usage: $0 basename"
	echo "e.g. $0 140118-test"
	exit
fi

# common protocol record intro columns:
MPHEAD="mp_cmd,mp_cmdres,mp_row,mp_rowcnt,rec_type,rec_time"

echo "Fetch RT-ENG-LogKeyTime..."
(
	echo "${MPHEAD},0,KeyHour,KeyMinSec"
	./cmd.pl 32 "RT-ENG-LogKeyTime"
) > "${NAME}-LogKeyTime.csv"

echo "Fetch RT-ENG-LogAlerts..."
(
	echo "${MPHEAD},EntryNr,Code,Description"
	./cmd.pl 32 "RT-ENG-LogAlerts"
) > "${NAME}-LogAlerts.csv"

echo "Fetch RT-ENG-LogFaults..."
(
	echo "${MPHEAD},EntryNr,Code,Description,TimeHour,TimeMinSec,Data1,Data2,Data3"
	./cmd.pl 32 "RT-ENG-LogFaults"
) > "${NAME}-LogFaults.csv"

echo "Fetch RT-ENG-LogSystem..."
(
	echo "${MPHEAD},EntryNr,Code,Description,TimeHour,TimeMinSec,Data1,Data2,Data3"
	./cmd.pl 32 "RT-ENG-LogSystem"
) > "${NAME}-LogSystem.csv"

echo "Fetch RT-ENG-LogCounts..."
(
	echo -n "${MPHEAD},EntryNr,Code,Description,LastTimeHour,LastTimeMinSec"
	echo ",FirstTimeHour,FirstTimeMinSec,Count"
	./cmd.pl 32 "RT-ENG-LogCounts"
) > "${NAME}-LogCounts.csv"

echo "Fetch RT-ENG-LogMinMax..."
(
	echo -n "${MPHEAD},EntryNr,BatteryVoltageMin,BatteryVoltageMax"
	echo -n ",CapacitorVoltageMin,CapacitorVoltageMax,MotorCurrentMin,MotorCurrentMax"
	echo ",MotorSpeedMin,MotorSpeedMax,DeviceTempMin,DeviceTempMax"
	./cmd.pl 32 "RT-ENG-LogMinMax"
) > "${NAME}-LogMinMax.csv"

echo "Done."

ls -l ${NAME}-*
