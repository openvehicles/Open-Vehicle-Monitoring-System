#!/bin/bash
#
# V 0.1
#
# This is Freeware - Use at your own risk
#  Question? freeware@meinbriefkasten.eu or german Twizy-forum http://www.vectrix-forum.de/
#
# Check if vehicle is home and chage is less than defined threshold
#  if yes switch on powerplug to charge
#  if no switch of powerplug to charge
#  if vehicle leaves home before fully charged switch of powerplug
#
# Coordinated and IDs für Homematic are to be defined in config.txt
#

now=$(date +"%Y%m%d")
LogFile="/tmp/${now}_ovms.log"
/usr/bin/touch "$LogFile"
echo "--------------------------------------" | tee -a $LogFile

# Log current dateTime 
echo "$(/bin/date) - Check started" | tee -a $LogFile

# Read Configuration (Home coordinates, homematic device IDs)
source config.txt || exit

# import Homematic-functions
source Homematic.sh || exit

# Read thresholds für SOC
SOC_LowerThreshold=$(HMGetNumber $HOMEMATIC_IP $AccuLowerThreshold_ID)
echo "SOC Lower Threshold Schwellwert = ${SOC_LowerThreshold}%" | tee -a $LogFile

SOC_UpperThreshold=$(HMGetNumber $HOMEMATIC_IP $AccuUpperThreshold_ID )
echo "SOC Upper Threshold = ${SOC_UpperThreshold}%" | tee -a $LogFile

# Read current SOC:
SOC=$(./status.pl | sed 's:MP-0 S\([0-9]*\).*:\1:g')
echo "SOC current charge = ${SOC}%" | tee -a $LogFile

# Read current Homematic switch state:
STATE=$(HMGetStringBoolean $HOMEMATIC_IP $SwitchTwizy_ID)
echo "Homematic current Switch state = ${STATE}" | tee -a $LogFile

# READ TWIZY Location (Result is home or ontheroad)
LOCATION=$(./location.sh)
echo "Current location of vehicle = ${LOCATION}" | tee -a $LogFile

if [[ $LOCATION = "home" ]] ; then
	# Switch Homematic Twizy Switch according to SOC level:
	if [[ $SOC -ge $SOC_UpperThreshold && $STATE = "true" ]] ; then
		echo "SOC level ${SOC}%, switching OFF... "| tee -a $LogFile 
		$(HMSwitch $HOMEMATIC_IP $SwitchTwizy_ID false)
	elif [[ $SOC -le $SOC_LowerThreshold && $STATE = "false" ]] ; then
		echo "SOC level ${SOC}%, switching ON... "| tee -a $LogFile 
		$(HMSwitch $HOMEMATIC_IP $SwitchTwizy_ID true)
	fi
else
	if [[ $STATE = "true" ]] ; then
		echo "Turn off switch" i| tee -a $LogFile
		$(HMSwitch $HOMEMATIC_IP $SwitchTwizy_ID false)
	fi
fi
