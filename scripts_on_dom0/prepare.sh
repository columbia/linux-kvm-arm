#!/bin/bash
domU_ID=`sudo xl list | grep DomU1 | awk '{ print $2 }'`
echo $domU_ID
sudo ./pin.sh "$domU_ID"
sudo ./probe_2.sh vsimple "$domU_ID"
