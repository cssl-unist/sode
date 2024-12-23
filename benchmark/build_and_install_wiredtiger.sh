#!/bin/bash

# Install build dependencies
SCRIPT_PATH=`realpath $0`
BASE_DIR=`dirname $SCRIPT_PATH`
WT_PATH="$BASE_DIR/wiredtiger"

# Build WiredTiger
pushd $WT_PATH

printf "Building WiredTiger...\n"
if [ ! -e "$WT_PATH/wt" ]; then
    ./autogen.sh
    ./configure
    make -j8
fi
sudo make install
popd

# Build WiredTiger BPF program
pushd $WT_PATH/bpf_prog
printf "Building WiredTiger BPF program...\n"
make
popd
