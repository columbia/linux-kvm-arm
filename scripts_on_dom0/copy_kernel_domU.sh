#!/bin/bash
IP=${1-"10.10.1.120"}
rsync -a /lib/modules/`uname -r` root@$IP:/lib/modules/.
scp /boot/*`uname -r`* root@$IP:/boot/.
