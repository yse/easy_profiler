#!/bin/bash

DISABLED_PROF=./bin/profiler_sample_disable
ENABLED_PROF=./bin/profiler_sample_enable

DISABLE_FILE=disable.info
ENABLE_FILE=enable.info

for i in {1..100} 
do 
	$DISABLED_PROF >> $DISABLE_FILE
	$ENABLED_PROF >> $ENABLE_FILE
done

DISABLE_AVERAGE_TIME=`awk '{s+=$1}END{print s/NR}' RS=" " $DISABLE_FILE`
ENABLE_AVERAGE_TIME=`awk '{s+=$1}END{print s/NR}' RS=" " $ENABLE_FILE`

DT=`echo "$ENABLE_AVERAGE_TIME - $DISABLE_AVERAGE_TIME" | bc -l`
PERCENT=`echo "$DT*100.0/$ENABLE_AVERAGE_TIME" | bc -l`
echo "dT: $DT usec"
echo "percent: $PERCENT%"
