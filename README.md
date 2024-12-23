# Selective On-Device Execution of Data-Dependent Read I/Os (SODE)

## Introduction
This repository contains source code and benchmarks to reproduce key results in the SODE paper (to appear in FAST 25).

The implementation of SODE is based on [NVMeVirt-b4c3c9d](https://github.com/snu-csl/nvmevirt/tree/b4c3c9d) and [XRP-9be399a](https://github.com/xrp-project/XRP/tree/9be399a). Currently, SODE's emulator supports and tests NVM SSDs mode only (or you can configure other storage types NVMeVirt support).


Further details on the design and implementation of SODE can be found in the following paper.
- [FAST 2025, Selective On-Device Execution of Data-Dependent Read I/Os](etc/fast25-sode.pdf)

## Getting Started Instructions

### Hardware Requirements
SODE requires three hardware components:
1. Two or more CPU nodes\
    **Your system must have two nodes at least because one is used for applications, and the other is used for computational storage emulation**. Currently, SODE is tested on two-node systems (Node 1 for applications and Node 2 for computational storage emulation).
2. Emulating wimpy on-device cores\
    Some cores used by on-device computations need CPU frequency scaling to emulate 
    whimpy on-device cores. **First, you must select 9 cores (5 for storage emulation + 4 for on-device computations) at least for evaluations of SODE, and change [disable_cpu_freq_scaling.sh](utils/disable_cpu_freq_scaling.sh) script to fix the frequency of 4 cores for on-device computations**. Currently, CPU 17, 18, 19, and 20 are used for on-device computations. Other CPU 12, 13, 14, 15, and 16 are used for storage emulations. **Second, you must specify and set [NUM\_R\_CPU](src/emulator/nvmev.h) (Number of On-device CPUs) value also**. In other words, your system must have a CPU node for emulation with 9 cores. For a detailed explanation, see [SODE paper, 4.2 Emulating Wimpy On-device Cores](etc/fast25-sode.pdf).
3. Large memory bound to the node for computational storage emulation\
    At least, we recommend 192GB of memory bound to the node for computational storage emulation because the YCSB benchmark requires large disk space.
4. Other requirements\
    We recommend to **turn off hyper-threading, processor C-states, and turbo boost**.

Tested SODE hardware configuration (or [see SODE paper, 5 Evaluation](etc/fast25-sode.pdf)):
|   Hardware    | Product | Description |
|---------------|---------|-------------|
|   CPU         |   Two Intel Xeon Gold 6136, 3.00GHz | Low CPU Frequency 1.20GHz, NUMA Configurations |
|   Motherboard |   Supermicro X11DPi-N |   two NUMA nodes, each having 12 cores and 192GB of memory |
| Memory        |   Samsung DDR4 M393A4K40BB2-CTD 32GB | 192GB (32GBx6) + 192GB (32GBx6) |

### Instructions
#### Software Dependency
First, clone this repository in a folder that has large disk space to compile Lthe inux kernel:
```
git clone https://github.com/blazer502/FAST25_SODE.git
cd FAST25_SODE
```

Second, run the script to install specific yaml, clang, llvm, and python:
```
cd benchmark/
./init_env.sh
```
* yaml is used to build YCSB.
* clang and llvm are used to compile BPF programs
* We recommand use python3.6 to run [the script](benchmark/My-YCSB/script/zipfian_trace.py) due to fstring usages.

#### Build SODE kernel
First, compile and install a custom kernel (This process will require some time to install dependencies and compile the kernel from scratch):
```
cd src/kernel/
./build_and_install_kernel.sh
```

Second, reboot with the compiled kernel:
```
cd utils/
./reboot.sh

# After reboot, check the kernel is changed
uname -r
```

#### Build and Setup Computational Storage Device Emulator

First, modify the following option to reserve physical memory to bind specific memory to the node used for emulation. This configuration assumes 192GB memory from the offset 192GB is reserved. And, your system must disable interrupt remapping (`intremap`) and intel I\O MMU service (`intel_iommu`). You can modify the setup:
```
# memmap=<memory size>\\<memory offset>
GRUB_CMDLINE_LINUX="... memmap=192G\\192G intremap=off intel_iommu=off"
```

Second, update grub and reboot your system:
```
sudo update-grub

# Reboot to SODE kernel
cd utils/
sudo ./reboot.sh 
```

Third, compile SODE emulator:
```
cd src/emulator/
make
```

Fourth, load SODE emulator with 5 core numbers. The default cores for storage I/Os are 13, 14, 15, and 16. Then, the emulator setup cores for computational emulation are 17, 18, 19, and 20. The rest of core 12 is used for dispatching I/O requests:
```
# sudo insmod ./nvmev.ko \
#               memmap_start=<memory offset> \
#               memmap_size=<memory size> \
#               cpus=<1+4 core numbers>

sudo insmod ./nvmev.ko \
    memmap_start=192G \
    memmap_size=192G \
    cpus=12,13,14,15,16
```

Lastly, you can see with `dmesg`:
```
[  455.505177] NVMeVirt: Version 1.10 for >> NVM SSD <<
[  455.505186] NVMeVirt: Storage: 0x3000100000-0x6000000000 (196607 MiB)
[  455.744866] NVMeVirt: ns 0/1: size 196607 MiB
[  455.745475] NVMeVirt: Virtual PCI bus created (node 1)
[  455.753137] NVMeVirt: nvmev_io_worker_0 started on cpu 13 (node 1)
[  455.753143] NVMeVirt: resubmit started on cpu 19 (node 1)
[  455.753179] NVMeVirt: nvmev_io_worker_1 started on cpu 14 (node 1)
[  455.753180] NVMeVirt: resubmit started on cpu 17 (node 1)
[  455.753200] NVMeVirt: nvmev_io_worker_2 started on cpu 15 (node 1)
[  455.753202] NVMeVirt: nvmev_io_worker_3 started on cpu 16 (node 1)
[  455.753216] NVMeVirt: resubmit started on cpu 18 (node 1)
[  455.753363] NVMeVirt: nvmev_dispatcher started on cpu 12 (node 1)
[  455.754004] NVMeVirt: Virtual NVMe device created
[  455.754657] NVMeVirt: resubmit started on cpu 20 (node 1)
```


#### Build Benchmarks
First, build and install BPF-KV:
```
cd benchmark/
./build_and_install_bpfkv.sh
```

Second, build and install WiredTiger and My-YCSB:
```
cd benchmark/
./build_and_install_wiredtiger.sh
./build_and_install_wiredtiger-sode.sh
./build_and_install_wiredtiger-sode-noparallel.sh
./build_and_install_ycsb.sh
```

Lastly, you can check the functionality of SODE on BPF-KV and WiredTiger:
```
cd eval/test/
./test_bpfkv.sh /dev/nvme0n1
./test_wiredtiger.sh /dev/nvme0n1
```

#### Run All Benchmarks
Run the script with the emulator device name (e.g. /dev/nvme0n1):
```
cd eval/
./run.sh /dev/nvme0n1
```

#### Generate Figures

Simply use `figure.sh` after running all benchmarks.
```
cd eval/
./figure.sh
```

Here is a list of the key results in this paper:
* Figure 5(a): 99-th percentile latency of BPF-KV with index depth 6
* Figure 5(b): 99.9-th percentile latency of BPF-KV with index depth 6
* Figure 5(c): 99.9-th percentile latency of BPF-KV for varying I/O index depth with single thread
* Figure 5(d): Average latency for varying I/O index depth with single thread
* Figure 6(a): Multi-threaded throughput of BPF-KV with index depth 6
* Figure 6(b): Multi-threaded throughput of BPF-KV with index depth 3
* Figure 6(c): Single-threaded throughput of BPF-KV for varying I/O index depth
* Figure 7(a): Average latency of range query in BPF-KV
* Figure 7(b): Throughput of range query in BPF-KV
* Figure 8(a): Throughput of BPF-KV using an open-loop load generator
* Figure 8(b): Latency-throughput graph of BPF-KV with 12 threads
* Figure 9: Throughput of WiredTiger for varying client threads
* Figure 10: 99-th percentile latency of WiredTiger for varying client threads
* Figure 11: Throughput speedup of WiredTiger for varying skewness
* Figure 12: Normalized latency reduction of SODE
* Figure 13: Normalized throughput of WiredTiger on YCSB-C
* Appendix Figure 2: Throughput of WiredTiger for varying cache sizes


## Authors
- Chanyoung Park (UNIST)    chanyoung@unist.ac.kr
- Minu Chung (UNIST)        minu0122@unist.ac.kr
- Hyungon Moon (UNIST)      hyungon@unist.ac.kr

## Publication
```
Not Yet
```
