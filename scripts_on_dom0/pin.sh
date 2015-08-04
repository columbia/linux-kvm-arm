#!/bin/bash
echo "in pin.sh"
echo $1
sudo xl vcpu-pin 0 0 0
sudo xl vcpu-pin 0 1 1
sudo xl vcpu-pin 0 2 2
sudo xl vcpu-pin 0 3 3
sudo xl vcpu-pin "$1" 0 4
sudo xl vcpu-pin "$1" 1 5
sudo xl vcpu-pin "$1" 2 6
sudo xl vcpu-pin "$1" 3 7
sudo xl vcpu-list
