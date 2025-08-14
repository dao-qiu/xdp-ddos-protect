[![Test XDP program compile/load/attach on Ubuntu](https://github.com/SRodi/xdp-ddos-protect/actions/workflows/test.yml/badge.svg)](https://github.com/SRodi/xdp-ddos-protect/actions/workflows/test.yml)

# Protect from DDoS attacks with XDP 

The [xdp_ddos_protection.c](./xdp_ddos_protection.c) file contains an eBPF program designed for DDoS protection using XDP (eXpress Data Path).

This README.md also contains simple instructions to simulate a `SYN flood attack` on the lo interface by running a local server used as a target, which listens on a given TCP port, and a client which sends many SYN packets to the target.

![Demo](./static/demo.gif)

## Program logic

The `eBPF XDP program` includes the necessary Linux kernel headers and defines constants for rate limiting, such as the maximum packets per second (`THRESHOLD`) and the time window in nanoseconds (`TIME_WINDOW_NS`).The program maintains a hash map (`rate_limit_map`) to track the rate limit for each source IP address, storing the last update timestamp and packet count within the time window.

The ddos_protection function, marked with the `SEC("xdp")` section, processes incoming packets, starting by parsing the `Ethernet header`. It checks if the packet is an `IP packet` and ensures that the packet data is within bounds before proceeding with further processing. If the packet is not an IP packet or the data is out of bounds, it passes the packet without any action.

If none of the above conditions are met, the function then extracts the source IP address from the IP header of the incoming packet. It then looks up a rate limit entry for this IP address in the `rate_limit_map` BPF map.

The current time is fetched in nanoseconds using `bpf_ktime_get_ns()`. If an entry for the source IP exists, the code checks if the current time is within the same time window defined by `TIME_WINDOW_NS`. If it is, the packet count for this IP is incremented. If the packet count exceeds a predefined `THRESHOLD`, the packet is dropped by returning `XDP_DROP`. If the time window has elapsed, the packet count is reset, and the time of the last update is set to the current time.

If no entry exists for the source IP, a new rate limit entry is initialized with the current time and a packet count of one. This new entry is then added to the rate_limit_map. If the packet count is within the threshold, the packet is allowed to pass by returning `XDP_PASS`. This mechanism helps in mitigating DDoS attacks by limiting the rate of packets from any single IP address.

## Compatibility

This program is compatible for both `amd64` and `arm64` architectures. The program was tested on Ubuntu/Debian Linux distributions with `kernel version >= 5.15`.

![arm64](./static/arm64.png)

## Repository files

* [Makefile](./Makefile): help to compile, attach, and detach the BPF program
* [README.md](./README.md): provides instructions and documentation
* [xdp_ddos_protection.c](./xdp_ddos_protection.c): eBPF program for DDoS protection using XDP

## Installation

1. . **Install clang, llvm, libbpf and make**:
    ```sh
    sudo apt-get install clang llvm libbpf-dev make -y
    ```
2. **Clone the repository**:
    ```sh
    git clone https://github.com/srodi/xdp-ddos-protect.git
    cd xdp-ddos-protect
    ```

## Compile BPF code
To compile BPF XDP program, run:
```sh
make compile
```

## Attach the BPF program
To attach the program to `lo` interface, use:
```sh
make attach
```

To attach to another interface:
```sh
make attach IFACE=eth0
```

## Detach program
To detach the BPF program
```sh
make detach
```

## Test

To test a DDoS attack, you can use `hping3` tool, here is a script
```sh
sudo apt install hping3
```

Run an `nginx` server locally using `docker`:
```sh
docker run -p 1234:80 nginx
```
Then run the test:
```sh
sudo hping3 -i u1000 -S -p 1234 127.0.0.1
```

The `-i u1000` option in the `hping3` command specifies the interval between packets. Here, `u1000` means 1000 microseconds (1 millisecond) between each packet. This option is used to control the rate of packet sending, which is useful for simulating high traffic or DDoS attacks.

The `-S` option in the `hping3` command specifies that the SYN flag should be set in the TCP packets. This is used to simulate a SYN flood attack, which is a type of DDoS attack where many SYN packets are sent to a target to overwhelm it.

## Troubleshooting

**Add the correct include path**
If the header files `types.h` are located in a different directory than `/usr/include/asm`, you can add the include path manually via environment variable `C_INCLUDE_PATH`:

```sh
# ensure asm/types.h exists in the expected directory
find /usr/include -name 'types.h'

# example for Ubuntu 24.04 x86_64 
export C_INCLUDE_PATH=/usr/include/x86_64-linux-gnu
```

**Disable LRO**
If you're encountering an issue with attaching an XDP program to the eth0 interface with a similar error to the following, this is due to Large Receive Offload (LRO) being enabled.

```sh
$ sudo ip link set dev eth0 xdp obj xdp_ddos_protection.o sec xdp
Error: hv_netvsc: XDP: not support LRO.
```
The error message indicates that the hv_netvsc driver does not support XDP when LRO is enabled. To resolve this issue, you need to disable LRO on the eth0 interface before attaching the XDP program. You can do this using the ethtool command:

```sh
sudo ethtool -K eth0 lro off
```

## Considerations

The test presented above uses `lo` which is a virtual network interface - by default, XDP is designed to work on physical and virtual interfaces that send and receive packets from the network. The loopback interface behaves differently because it is a purely software interface used for local traffic within the system.

Traffic on the loopback interface is not "real" network traffic, it is handled entirely within the kernel. As a result, certain packet processing steps (like those involving hardware offload) are bypassed, and this can affect how XDP interacts with the loopback interface.

The performance benefits of XDP may be less pronounced for loopback traffic compared to physical interfaces. This means the performance is greater on regular network interfaces like `eth0`, which represent a physical hardware device.

## How to run on Texas Instruments Processors

The steps outline below are in conjuction with the presentation on "Strategies for Rate Limiting Network Packet Ingress" for the Open Source Summit Europe (OSS-E) conference in August 2025.  
  
The main difference of the steps outlined here versus the original xdp-ddos-protect project is the addition of how to load the eBPF program on a Texas Instruments (TI) embedded processor (uses arm64 architecture) using a physical hardware device.  

As part of the changes made, the rate limit threshold was changed from packet per second to bit per second for easier evaluation using the iperf3 utility tool.
  
Additionally, the performance of running an XDP based rate-limiting scheme using eBPF is evaluated via monitoring the CPU load on the TI processor.

### `xdp-rate-limit-v5.c` eBPF XDP program
The `xdp-rate-limit-v5.c` eBPF XDP program is adapted from the original `xdp-ddos-protect.c` eBPF XDP program to drop packets based on bit rate rather than packet rate. This was done for easier evaluation using the `iperf3` utility tool which outputs throughput statistics based on megabits per second (Mbps).  

In this particular example, if the number of bits accumulated during the 1 second time window is greater than 10Mbps, packets will be dropped until the next time window.   

### Compiling the `xdp-rate-limit-v5.c` program
The compilation of the `xdp-rate-limit-v5.c` program will be done on a desktop machine running Ubuntu with internet connection. The reason for this is because the required packages for compiling an eBPF program can be easily obtained. The Linux distribtuion running on a TI embedded processor is a Yocto build designed for an embedded platform so it is more difficult to easily install all packages necessary. Since eBPF programs are built into eBPF bytecode, which is not architecture agnostic, building on a desktop machine and then transferring the eBPF bytecode to the target platform works.  

The packages required for compilation have already been specified by the original project. To summarize, the `llvm`, `clang`, `libbpf`, `libxdp`, and `libelf` packages are required. Additionally, the Linux kernel headers should verified to be installed. Please see https://github.com/xdp-project/xdp-tutorial/blob/main/setup_dependencies.org for more details.

To compile the program, please run `clang -O2 -g -target bpf -c xdp-rate-limit-v5.c -o xdp-rate-limit-v5.o`. The output file `xdp-rate-limit-v5.o` is the eBPF bytecode that needs to be transferred to the target embedded platform.  

### Preparing the TI Processor
For the OSS-E conference, the TMDS64EVM evaluation board featuring the AM6442 arm-based TI processor is used as the embedded platform through which rate-limiting techniques is evaluated. The TMDS64EVM will need an at least 16GB microSD card flashed with the default RT-Linux wic image which can be found at https://www.ti.com/tool/download/PROCESSOR-SDK-LINUX-RT-AM64X. For OSS-E, version 11.01.05.03 is used.  
  
In addition, the kernel configuration flashed on the SD card has to be modified to include the following kernel configs.
`CONFIG_BPF`  
`CONFIG_BPF_SYSCALL`  
`CONFIG_BPF_JIT`  
`CONFIG_BPF_EVENTS`  
`CONFIG_TRACING`  
`CONFIG_FTRACE`  
`CONFIG_FUNCTION_TRACER`  
  
RT-Linux wic image should already include the `ip` Linux utility tool which is used later to load the eBPF bytecode.  

### Loading the eBPF bytecode
Once the `xdp-rate-limit-v5.o` eBPF bytecode is copied or transfered over to the TI EVM, either via `scp` or directly copying to the SD card, it needs to be loaded using `ip link set dev eth0 xdp obj xdp-rate-limit-v5.o sec xdp`, where `eth0` is the interface name of the Ethernet port that is tested. As mentioned, the `ip` utility tool should already be installed in the RT-Linux wic image.  
  
Note that the `xdp` option can be used because the NIC driver on the AM6442 (called Common Platform Switch or CPSW) supports native XDP. If the NIC driver does not support native XDP, `xdpgeneric` can alternatively be used. However, `xdpgeneric` does not offer many performance improvements.  

Once loaded you should see the `eth0` link, if already connected with link up to another device, be brought down and back up again. See below for an example of what you should see.  
```
root@am64xx-evm:~/ingress-rate-limit/ebpf-method# ip link set dev eth0 xdp obj xdp-rate-limit-v5.o sec xdp
[139928.625430] audit: type=1334 audit(1750712479.877:15): prog-id=7 op=LOAD
[139928.625464] audit: type=1300 audit(1750712479.877:15): arch=c00000b7 syscall=280 success=yes exit=6 a0=5 a1=fffff5bdb080 a2=90 a3=fffff5bdb080 items=)
[139928.625480] audit: type=1327 audit(1750712479.877:15): proctitle=6970006C696E6B0073657400646576006574683000786470006F626A007864702D726174652D6C696D690
[139928.626480] am65-cpsw-nuss 8000000.ethernet eth0: Link is Down
[139928.646740] am65-cpsw-nuss 8000000.ethernet eth0: PHY [8000f00.mdio:00] driver [TI DP83867] (irq=POLL)
[139928.646772] am65-cpsw-nuss 8000000.ethernet eth0: configuring for phy/rgmii-rxid link mode
root@am64xx-evm:~/ingress-rate-limit/ebpf-method# [139932.774121] am65-cpsw-nuss 8000000.ethernet eth0: Link is Up - 1Gbps/Full - flow control rx/tx
```

If you want to unload the program, simply run `ip link set dev eth0 xdp off` and you should see something like the following show up.  
```
root@am64xx-evm:~/ingress-rate-limit/ebpf-method# ip link set dev eth0 xdp off
[140021.343153] am65-cpsw-nuss 8000000.ethernet eth0: Link is Down
[140021.360773] am65-cpsw-nuss 8000000.ethernet eth0: PHY [8000f00.mdio:00] driver [TI DP83867] (irq=POLL)
[140021.360802] am65-cpsw-nuss 8000000.ethernet eth0: configuring for phy/rgmii-rxid link mode
[140021.382215] audit: type=1334 audit(1750712572.595:16): prog-id=7 op=UNLOAD
[140021.383682] audit: type=1300 audit(1750712572.595:16): arch=c00000b7 syscall=211 success=yes exit=52 a0=3 a1=ffffeea59508 a2=0 a3=1 items=0 ppid=885 )
[140021.383714] audit: type=1327 audit(1750712572.595:16): proctitle=6970006C696E6B0073657400646576006574683000786470006F6666
root@am64xx-evm:~/ingress-rate-limit/ebpf-method# [140025.446013] am65-cpsw-nuss 8000000.ethernet eth0: Link is Up - 1Gbps/Full - flow control rx/tx
```

To double check if the eBPF program was loaded or removed, run `ip link show dev eth0`. You should see something like the below if the program is loaded. If the loaded was not loaded the `prog/xdp id 8 name  tag d0683137a0900a7c jited` would be missing. Note that the id number and tag number could be different from what is shown here.  
```
root@am64xx-evm:~/ingress-rate-limit/ebpf-method# ip link show dev eth0
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 xdp qdisc mq state UP mode DEFAULT group default qlen 1000
    link/ether 34:08:e1:80:a7:ad brd ff:ff:ff:ff:ff:ff
    prog/xdp id 8 name  tag d0683137a0900a7c jited
```

### Evaluating network throughput performance
To first test if the rate is being limiting, the `iperf3` utility using TCP packets are used to similate a large network bandwidth. Using TCP packets with `iperf3` means that `iperf3` will automatically send as close to the link speed/line rate as possible. On TMDS64EVM, a link speed/line rate of 1Gbps is used.  

Since close to 1Gbps is generated by `iperf3` and the eBPF program we use has a rate limit threshold of 10Mbps, we should see a drop in throughput observed on the TI EVM. For clarity, `iperf3` client will run on the device sending packets to the TI EVM and `iperf3` server runs on the TI EVM.  

### Evaluating CPU load performance
The main metric to evaluating performance improvement is to see if the load on the CPU cores was reduced with an eBPF XDP rate limiting scheme. To quickly evaluate CPU load, `mpstat -P ALL 1 1` can be used to view the load on the A53 CPU cores on the AM6442, updated once per second. `mpstat` is an utility tool that should already be installed in the RT-Linux wic image used for the TMDS64EVM.  

When the eBPF program is loaded, you should see a significant reduction in percent CPU load on all A53 cores when `iperf3` is running and packets are being dropped.  

### Steps used to collect data
To collect the performance data, two scripts are used: 1. `cpu-load-mpstat.sh` and 2. `start-capture-ebpf.sh`.  
  
`cpu-load-mpstat.sh` is meant to extract CPU load per core and store into a separate text file for each core. Since AM6442 has two A53 cores, the script extracts CPU load for just the two A53 cores.  
  
`start-capture-ebpf.sh` is meant to setup the `iperf3` server and run `cpu-load-mpstat.sh` for about 80 seconds. The output of `iperf3` server is also saved into a text file.  
  
Additionally, the device used to send packets to the TI EVM was the same desktop PC used for compiling the eBPF program. Wireshark was used on the desktop PC to monitor traffic from the `iperf3` client side.  
  
The full sequence of events to capture data is outlined below.  
1. Start Wireshark capture on the desktop PC
2. Start `start-capture-ebpf.sh` on the TI EVM
3. Start `iperf3` client on the desktop PC
4. Load the eBPF program after 15 seconds since the start of running `start-capture-ebpf.sh`
5. Unload the eBPF program after 70 seconds since the start of running `start-capture-ebpf.sh`
6. After `start-capture-ebpf.sh` stops running, stop Wireshark
7. Check and rename the output files from `start-capture-ebpf.sh` on the TI EVM and save those files
8. Check the I/O graph in Wireshark (Statistics--> I/O Graphs) and enable the Y axis to be Bits/1sec instead of packets/1sec
9. Ensure TCP errors are also seen/checked in the I/O graph and save as .csv file
10. Save the entire Wireshark capture as a pcap file  

The save files can then be used to plot graphs.  
  
## License
This project is licensed under the MIT License. See the LICENSE file for details.  
Original project is attributed to Simone Rodigari.  
Further modifications made in this project are done by Daolin Qiu, copyright rights go to Texas Instruments.
