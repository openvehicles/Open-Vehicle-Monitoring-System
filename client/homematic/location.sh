#!/bin/bash
#
# V 0.1
#
# This is Freeware - Use at your own risk
#  Question? freeware@meinbriefkasten.eu or german Twizy-forum http://www.vectrix-forum.de/
#
# INTENDED USE
#   Checks if our vehicle is at home. Home is defined as to be in 50m of the configured coordinates
#
# Returnvalue: 
#   "home"      - if Twizy is at home
#   "ontheroad" - if twizy is not at home
#
# required Inputparameters
#  in config.txt the GPS coordinated of the place you call home need to be defined
#


# Read config:
source config.txt || exit

# Read current Location
LOC_MSG=$(./status.pl L)
LONG=$(echo $LOC_MSG | sed 's:MP-0 L\([0-9.-]*\).*:\1:g')
LAT=$(echo $LOC_MSG | sed 's:MP-0 L[0-9.-]*,\([0-9.-]*\).*:\1:g')

IS_HOME=$(calc "( abs($LONG - $HOME_GIS_LONG) < 0.0004 ) && ( abs($LAT - $HOME_GIS_LAT) < 0.0004 )")

if [[ "$IS_HOME" -eq 1 ]] ; then
	RESULT="home"
else
	RESULT="ontheroad"
fi

# Output result:
echo -n $RESULT
