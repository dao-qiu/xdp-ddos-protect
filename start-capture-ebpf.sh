timeout 80 iperf3 -s > iperf3-stats-ebpf.txt &
./cpu-load-mpstat.sh > /dev/null 81 & 
