#!/bin/bash
for i in `seq 0 7`; do
	xl vcpu-pin Domain-0 0 $i 
	taskset 0x1 bash -c 'xl debug-keys C'
done
