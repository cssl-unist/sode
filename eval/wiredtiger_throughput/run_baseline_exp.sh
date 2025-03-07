if [ "$(uname -r)" !=  "5.12.0-sode" ]; then
    printf "Not in SODE kernel. Please run the following commands to boot into SODE kernel:\n"
    printf "    sudo grub-reboot \"Advanced options for Ubuntu>Ubuntu, with Linux 5.12.0-sode\"\n"
    printf "    sudo reboot\n"
    exit 1
fi

SCRIPT_PATH=`realpath $0`
EVAL_PATH=`dirname $SCRIPT_PATH`
BASE_DIR=`realpath $EVAL_PATH/../..`
WT_PATH="$BASE_DIR/benchmark/wiredtiger"
YCSB_PATH="$BASE_DIR/benchmark/My-YCSB"

DEV_NAME="/dev/nvme0n1"
if [ ! -z $1 ]; then
    DEV_NAME=$1
fi
printf "DEV_NAME=$DEV_NAME\n"

CACHED_DB=""
if [ ! -z $2 ]; then
    CACHED_DB=$2
fi
printf "CACHED_DB=$CACHED_DB\n"

pushd $BASE_DIR/benchmark
./build_and_install_wiredtiger.sh 1> /dev/null 2> /dev/null
./build_and_install_ycsb.sh 1> /dev/null 2> /dev/null
popd

# Check whether WiredTiger is built
if [ ! -e "$WT_PATH/wt" ]; then
    printf "Cannot find WiredTiger binary. Please build WiredTiger first.\n"
    exit 1
fi
# Check whether My-YCSB is built
if [ ! -e "$YCSB_PATH/build/init_wt" ]; then
    printf "Cannot find My-YCSB binary. Please build My-YCSB first.\n"
    exit 1
fi



for CONFIG in "ycsb_a.yaml" "ycsb_b.yaml" "ycsb_c.yaml" "ycsb_d.yaml" "ycsb_e.yaml" "ycsb_f.yaml"; do
    CACHE_SIZE=512
    for NUM_THREADS in 1 2 3; do
        # Evaluate WiredTiger with XRP
        $EVAL_PATH/run_baseline_single_exp.sh $CONFIG $CACHE_SIZE $NUM_THREADS y $DEV_NAME $CACHED_DB
        # Evaluate WiredTiger with read()
        $EVAL_PATH/run_baseline_single_exp.sh $CONFIG $CACHE_SIZE $NUM_THREADS n $DEV_NAME $CACHED_DB
    done
    
    NUM_THREADS=1
    for CACHE_SIZE in 1024 2048 4096; do
        # Evaluate WiredTiger with XRP
        $EVAL_PATH/run_baseline_single_exp.sh $CONFIG $CACHE_SIZE $NUM_THREADS y $DEV_NAME $CACHED_DB
        # Evaluate WiredTiger with read()
        $EVAL_PATH/run_baseline_single_exp.sh $CONFIG $CACHE_SIZE $NUM_THREADS n $DEV_NAME $CACHED_DB
    done
done

printf "Done. Results are stored in $EVAL_PATH/result\n"
