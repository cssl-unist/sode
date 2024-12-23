#!/bin/bash
SCRIPT_PATH=`realpath $0`
BASE_DIR=`dirname $SCRIPT_PATH`
WT_PATH="$BASE_DIR/wiredtiger"
YCSB_PATH="$BASE_DIR/My-YCSB"

if [ ! -e "$WT_PATH/wt" ]; then
    printf "Please build and install WiredTiger first.\n"
    exit 1
fi

# Build YCSB
pushd $YCSB_PATH

printf "Building My-YCSB...\n"
mkdir build
cd build
cmake ..
make init_wt
make run_wt
popd
