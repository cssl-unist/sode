#!/bin/bash

DEV_NAME=""
if [ -z $1 ]; then
    printf "Set the device path e.g. /dev/nvme0n1\n"
    exit
fi
DEV_NAME=$1
printf "DEV_NAME=$DEV_NAME\n"


printf "wiredtiger_skweness"

pushd wiredtiger_skewness
./run_baseline_exp.sh $DEV_NAME
./run_sode_exp.sh $DEV_NAME
popd

printf "wiredtiger_throughput\n"

pushd wiredtiger_throughput
./run_baseline_exp.sh $DEV_NAME
./run_sode_exp.sh $DEV_NAME
./run_sode_noparallel_exp.sh $DEV_NAME
popd

printf "wiredtiger_tail_latency\n"

pushd wiredtiger_tail_latency
./run_baseline_exp.sh $DEV_NAME
./run_sode_exp.sh $DEV_NAME
popd


printf "bpkv_thread_scaling\n"

pushd bpfkv_thread_scaling
./run_full_exp.sh $DEV_NAME
popd

printf "bpkv_multi_threads\n"

pushd bpfkv_multi_threads
./run_full_exp.sh $DEV_NAME
popd

printf "bpkv_range_query\n"

pushd bpfkv_range_query
./run_full_exp.sh $DEV_NAME
popd

printf "bpkv_single_thread\n"

pushd bpfkv_single_thread
./run_full_exp.sh $DEV_NAME
popd

printf "bpkv_throughput_latency\n"

pushd bpfkv_throughput_latency
./run_full_exp.sh $DEV_NAME
popd
