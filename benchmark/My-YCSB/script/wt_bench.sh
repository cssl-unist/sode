thread_count_list="1 2 3"
cache_size_list="4G 2G 1G 512M"
workload_list="ycsb_a ycsb_b ycsb_c ycsb_d ycsb_e ycsb_f uniform"

for workload in ${workload_list}
do
    if [ "$workload" = "uniform" ] || [ "$workload" = "ycsb_c" ]; then
        ./reset_db.sh
    fi

    # run baseline
    ~/disable_wt_ebpf.sh
    for thread_count in ${thread_count_list}
    do
        ./change_thread_count.sh ${thread_count}
        for cache_size in ${cache_size_list}
        do
            ./change_cache_size.sh ${cache_size}
            if [ "$workload" != "uniform" ] && [ "$workload" != "ycsb_c" ]; then
                ./reset_db.sh
            fi
            ./run_wt ../wiredtiger/config/${workload}.yaml | tee run_log/32G_${cache_size}_${thread_count}Thread_Unmodified_${workload^^}.txt
        done
    done

    # run BPF
    ~/enable_wt_ebpf.sh
    ../../ebpf/tools/build/init_syscall
    sudo dmesg -C
    for thread_count in ${thread_count_list}
    do
        ./change_thread_count.sh ${thread_count}
        for cache_size in ${cache_size_list}
        do
            ./change_cache_size.sh ${cache_size}
            if [ "$workload" != "uniform" ] && [ "$workload" != "ycsb_c" ]; then
                ./reset_db.sh
            fi
            ./run_wt ../wiredtiger/config/${workload}.yaml | tee run_log/32G_${cache_size}_${thread_count}Thread_BPF_${workload^^}.txt
            ../../ebpf/tools/build/init_syscall
            dmesg | tee run_log/32G_${cache_size}_${thread_count}Thread_BPF_${workload^^}_dmesg.txt
            sudo dmesg -C
        done
    done
done
