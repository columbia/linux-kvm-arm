#!/bin/bash
if [ "$#" -lt 1 ]; then
	echo "Usage: ./run.sh <test num> [iterations]"
	exit
fi
ID=$1
ITER=${2:-100}
for i in `seq 1 $ITER`
do
	echo $ID > /proc/virttest
done
