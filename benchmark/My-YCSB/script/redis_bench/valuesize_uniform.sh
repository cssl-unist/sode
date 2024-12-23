REDIS_ROOT=~/yuhong/redis
MY_YCSB_ROOT=~/yuhong/My-YCSB/build
MM_MEAUSRE_ROOT=~/yuhong/mm-measure/build
PERF_ROOT=~/yuhong/linux/tools/perf
LOG_ROOT=~/yuhong/exp_log

# setup redis config
sudo sysctl vm.overcommit_memory=1
sudo bash -c "echo never > /sys/kernel/mm/transparent_hugepage/enabled"
sudo bash -c "echo never > /sys/kernel/mm/transparent_hugepage/defrag"

# create log folder
mkdir -p $LOG_ROOT

WL_CONFIG=$MY_YCSB_ROOT/../redis/config/uniform.yaml

# run experiment
for value_size in 4096 16384 65536 262144
do
        # change config file
        sed -i "s/value_size: .*/value_size: "$value_size"/" $WL_CONFIG
        sed -i "s/nr_entry: .*/nr_entry: "$((1073741824 / $value_size))"/" $WL_CONFIG
        sed -i "s/nr_op: .*/nr_op: "$((10000000 * 4096 / $value_size))"/" $WL_CONFIG
        for color in 768 16 8 7 6 5 4
        do
                printf "[*] Running experiment with value size %d color %d\n" $value_size $color
                # kill redis
                while [ -n "$(pgrep redis)" ]
                do
                    kill $(pgrep redis)
                done
                printf "[*] Redis killed\n"

                # fill color
                $MM_MEAUSRE_ROOT/init_colormem 163840
                printf "[*] Color refilled\n"

                # start redis
                (numactl --cpunodebind 0  --membind 0 $MM_MEAUSRE_ROOT/colorctl 768 $color 0 $REDIS_ROOT/src/redis-server $REDIS_ROOT/redis.conf) &
                printf "[*] Redis restarted\n"

                sleep 5

                # run init
                numactl --cpunodebind 1 --membind 1 $MY_YCSB_ROOT/init_redis $WL_CONFIG
                printf "[*] Redis initialized\n"

                # start perf
                (sleep 60 && $PERF_ROOT/perf stat -o $LOG_ROOT/${value_size}_${color}_perf.log -e unc_m_tagchk.hit,unc_m_tagchk.miss_clean,unc_m_tagchk.miss_dirty,cycles,instructions -a --per-node sleep 5) &
                printf "[*] Perf started\n"
                
                # run workload
                (numactl --cpunodebind 1 --membind 1 $MY_YCSB_ROOT/run_redis $WL_CONFIG) | tee $LOG_ROOT/${value_size}_${color}_run.log
                
                # print perf
                cat $LOG_ROOT/${value_size}_${color}_perf.log
        done
done
