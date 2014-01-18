#!/bin/bash

echo "Query RT-ENG-LogAlerts..."
./cmd.pl 210 1

echo "Query RT-ENG-LogFaults..."
./cmd.pl 210 2,0
./cmd.pl 210 2,10
./cmd.pl 210 2,20
./cmd.pl 210 2,30

echo "Query RT-ENG-LogSystem..."
./cmd.pl 210 3,0
./cmd.pl 210 3,10

echo "Query RT-ENG-LogCounts..."
./cmd.pl 210 4

echo "Query RT-ENG-LogMinMax..."
./cmd.pl 210 5

echo "Done."
echo "Hint: use fetchlogs.sh to retrieve logs now."
