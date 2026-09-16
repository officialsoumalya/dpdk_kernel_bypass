# DPDK Market Data Adapter Benchmark

## 1. Mac Host Components

### Traffic Generator (`generator.cpp`)
Broadcasts 1000 simulated UDP market data ticks with nanosecond hardware timestamps.

**Compile Command:**
`clang++ -std=c++17 -O3 generator.cpp -o traffic_gen`

**Run Command:**
`./traffic_gen 192.168.128.255 9000`

---

### Latency Sniffer (`latency_sniffer.cpp`)
Captures echoed packets, calculates RTT, and outputs statistical percentiles on exit.

**Compile Command:**
`clang++ -std=c++17 -O3 latency_sniffer.cpp -o latency_sniffer -lpcap`

**Run Command:**
`sudo ./latency_sniffer bridge101`

---

## 2. Ubuntu VM Components

### Standard Kernel Receiver (`kernel_echo.cpp`)
Baseline standard POSIX socket server relying on the Linux networking stack.

**Compile Command:**
`g++ -O3 kernel_echo.cpp -o kernel_echo`

**Run Command:**
`./kernel_echo`

---

### DPDK Fastpath Receiver (`dpdk_echo.cpp`)
Kernel bypass application utilizing the Poll Mode Driver for zero-copy hardware polling.

**Compile Command:**
`g++ -O3 dpdk_echo.cpp -o dpdk_echo $(pkg-config --cflags --libs libdpdk)`

**Run Command (on isolated cores 0-2):**
`sudo ./dpdk_echo -l 0-2 --`

---

## 3. NIC Management (Ubuntu VM)

The virtual network interface (`enp0s2` / PCI slot `0000:00:02.0`) must be swapped between the Linux kernel and DPDK depending on the test.

### Option A: Attach NIC to Linux OS (Kernel Baseline)
Run this sequence before testing the standard kernel sockets.

1. **Bind to the standard kernel driver:**
   `sudo ./usertools/dpdk-devbind.py -b virtio-pci 0000:00:02.0`
2. **Restore network state and fetch IP:**
   `sudo netplan apply`
3. **Verify the IP assignment:**
   `ip a show enp0s2`

### Option B: Detach NIC from Linux OS (DPDK Fastpath)
Run this sequence before testing the DPDK kernel bypass application.

1. **Bring the interface down:**
   `sudo ip link set enp0s2 down`
2. **Load the user-space I/O module:**
   `sudo modprobe uio_pci_generic`
3. **Bind the hardware to the DPDK dummy driver:**
   `sudo ./usertools/dpdk-devbind.py -b uio_pci_generic 0000:00:02.0`
4. **Verify the hardware binding:**
   `./usertools/dpdk-devbind.py -s`

---

## 4. Full Execution Workflow

### Testing Kernel Sockets
1. (VM) Attach NIC to Linux OS.
2. (VM) Start the receiver: `./kernel_echo`
3. (Mac) Start the sniffer: `sudo ./latency_sniffer bridge101`
4. (Mac) Fire the generator: `./traffic_gen 192.168.128.255 9000`
5. (Mac) Press `Ctrl+C` on the sniffer to view statistics.

### Testing DPDK
1. (VM) Detach NIC from Linux OS.
2. (VM) Start the receiver: `sudo ./dpdk_echo -l 0-2 --`
3. (Mac) Start the sniffer: `sudo ./latency_sniffer bridge101`
4. (Mac) Fire the generator: `./traffic_gen 192.168.128.255 9000`
5. (Mac) Press `Ctrl+C` on the sniffer to view statistics.