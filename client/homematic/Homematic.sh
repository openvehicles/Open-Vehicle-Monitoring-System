#!/bin/bash
#
# V 0.1
#
# This is Freeware - Use at your own risk
#  Question? freeware@meinbriefkasten.eu or german Twizy-forum http://www.vectrix-forum.de/
#
# Access to homematic using Unix bash
# 
# required extension on homematic XML-API V 1.10 (please install, its free)
#

#
# function HMGetNumber()
#   Used to get a number-value of a Homematic device
#
# Parameter:
#  IP-Adress of Homematic
#  ID of device to be retrieved (get ID using HQ-WebUI extension in homematic)
#
function HMGetNumber()
{
	if [[ $# -lt 2 ]]; then
		Msg
		echo "Usage $0 IP-Adress_Homematic DeviceID"
		return 0
	fi
	
	# Get Edimax state:
	RESULT=$(  ( curl --silent http://$1/addons/xmlapi/state.cgi?datapoint_id=$2 ) | sed -e 's:.*value=.\([0-9]*\).*:\1:g' )
	
	echo -n $RESULT

	return 0
}


#
# function HMGetStringBoolean()
#   Used to get a string-value of a Homematic device
#
# Parameter:
#  IP-Adress of Homematic
#  ID of device to be retrieved (get ID using HQ-WebUI extension in homematic)
#
function HMGetStringBoolean()
{
	if [[ $# -lt 2 ]]; then
		Msg
		echo "Usage $0 IP-Adress_Homematic DeviceID"
		return ""
	fi

	RESULT=$(  ( curl --silent http://$1/addons/xmlapi/state.cgi?datapoint_id=$2 ) | sed -e 's:.*value=.\([truefalse]*\).*:\1:g' )

	echo -n $RESULT

	return 0
}


#
# function HMSwitch()
#   Used to get a string-value of a Homematic device
#
# Parameter:
#  IP-Adress of Homematic
#  ID of device to be switched (get ID using HQ-WebUI extension in homematic)
#  new value (false = turned off, true = turned on)
#
function HMSwitch()
{
	if [[ $# -lt 3 ]]; then
		echo "Usage $0 IP-Adress_Homematic DeviceID newValue[true,false]"
		return ""
	fi
	
	TOSTATE=$3
	if [[ "${TOSTATE}" = "" ]] ; then
		echo "Usage $0 IP-Adress_Homematic DeviceID newValue[true,false]"
		return ""
	fi
	
	# Switch Homematic and read result:
	RESULT=$( ( curl --silent http://$1/addons/xmlapi/statechange.cgi?ise_id=$2\&new_value=$TOSTATE )  | sed -e 's:.*new_value=.\([truefalse]*\).*:\1:g' )
	
	echo -n $RESULT

	return 0
}


