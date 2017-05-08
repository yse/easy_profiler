#!/bin/bash
TEMP_FILE_ENABLE="enable.info"
TEMP_FILE_DISABLE="disable.info"
OBJECTS="1000"

$CXX_COMPILER -O3 -std=c++11 -I../easy_profiler_core/include/ -L../bin/ -leasy_profiler  express_sample.cpp -o express_test_disabled
$CXX_COMPILER -O3 -std=c++11 -I../easy_profiler_core/include/ -L../bin/ -leasy_profiler -DBUILD_WITH_EASY_PROFILER express_sample.cpp -o express_test_enabled

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../bin

./express_test_disabled $OBJECTS > $TEMP_FILE_DISABLE
./express_test_enabled $OBJECTS > $TEMP_FILE_ENABLE

DT_ENA=`cat $TEMP_FILE_ENABLE | grep Elapsed| awk '{print $3}'`
N_ENA=`cat $TEMP_FILE_ENABLE | grep Blocks| awk '{print $3}'`
DT_DIS=`cat $TEMP_FILE_DISABLE | grep Elapsed| awk '{print $3}'`

DELTA=$(($DT_ENA-$DT_DIS))
USEC_BLOCK=`awk "BEGIN{print $DELTA/$N_ENA}"`

echo "~" $USEC_BLOCK "usec/block"
