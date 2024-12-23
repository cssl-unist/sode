#!/bin/bash

if [ "$(uname -r)" !=  "5.12.0-sode" ]; then
    printf "Not in SODE kernel. Please run the following commands to boot into SODE kernel:\n"
    printf "    sudo grub-reboot \"Advanced options for Ubuntu>Ubuntu, with Linux 5.12.0-sode\"\n"
    printf "    sudo reboot\n"
    exit 1
fi

# Install build dependencies
printf "Installing dependencies...\n"
sudo apt-get update
sudo apt-get install -y gcc-multilib clang llvm libelf-dev libdwarf-dev cmake

wget -O /tmp/libbpf0_0.1.0-1_amd64.deb https://old-releases.ubuntu.com/ubuntu/pool/universe/libb/libbpf/libbpf0_0.1.0-1_amd64.deb
wget -O /tmp/libbpf-dev_0.1.0-1_amd64.deb https://old-releases.ubuntu.com/ubuntu/pool/universe/libb/libbpf/libbpf-dev_0.1.0-1_amd64.deb
wget -O /tmp/dwarves_1.17-1_amd64.deb https://old-releases.ubuntu.com/ubuntu/pool/universe/d/dwarves-dfsg/dwarves_1.17-1_amd64.deb

sudo dpkg -i /tmp/libbpf0_0.1.0-1_amd64.deb
sudo dpkg -i /tmp/libbpf-dev_0.1.0-1_amd64.deb
sudo dpkg -i /tmp/dwarves_1.17-1_amd64.deb


SCRIPT_PATH=`realpath $0`
BASE_DIR="`dirname $SCRIPT_PATH`/../"
BPFKV_PATH="$BASE_DIR/benchmark/BPF-KV"
UTILS_PATH="$BASE_DIR/utils"

DEV_NAME="/dev/nvme0n1"
if [ ! -z $1 ]; then
    DEV_NAME=$1
fi
printf "DEV_NAME=$DEV_NAME\n"

# For specialized BPF-KV
BPFKV_IO_URING_PATH="$BASE_DIR/benchmark/Specialized-BPF-KV/io_uring"
BPFKV_IO_URING_OPEN_LOOP_PATH="$BASE_DIR/benchmark/Specialized-BPF-KV/io_uring_open_loop"
BPFKV_IO_URING_OPEN_LOOP_PATH_SODE="$BASE_DIR/benchmark/Specialized-BPF-KV/io_uring_open_loop_sode"

$UTILS_PATH/build_and_install_liburing.sh

# Build BPF-KV
pushd $BPFKV_PATH
printf "Building BPF-KV...\n"
make
popd

pushd $BPFKV_IO_URING_OPEN_LOOP_PATH
sed -i 's|#define DB_PATH .*|#define DB_PATH "'$DEV_NAME'"|' db-bpf.h
cmake .
make db-bpf
# Copy BPF program
cp $BPFKV_PATH/xrp-bpf/get.o .
popd

pushd $BPFKV_IO_URING_OPEN_LOOP_PATH_SODE
sed -i 's|#define DB_PATH .*|#define DB_PATH "'$DEV_NAME'"|' db-bpf.h
cmake .
make db-bpf
# Copy BPF program
cp $BPFKV_PATH/sode-bpf/get.o .
popd
