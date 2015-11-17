#!/bin/bash
for i in {0..7}
do
	xl vcpu-pin 0 $i $i
done
