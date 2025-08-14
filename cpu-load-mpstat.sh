#!/bin/bash

timeout $1 mpstat -P 0 1 | tee cpu0-load-raw-stats.txt &
timeout $1 mpstat -P 1 1 | tee cpu1-load-raw-stats.txt 

awk '$11 ~ /[0-9]/ {print 100-$11}' cpu0-load-raw-stats.txt | tee cpu0-load-stats.txt
awk '$11 ~ /[0-9]/ {print 100-$11}' cpu1-load-raw-stats.txt | tee cpu1-load-stats.txt
