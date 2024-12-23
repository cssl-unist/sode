if [ "$(uname -r)" !=  "5.12.0-sode" ]; then
    printf "Not in SODE kernel. Please run the following commands to boot into SODE kernel:\n"
    printf "    sudo grub-reboot \"Advanced options for Ubuntu>Ubuntu, with Linux 5.12.0-sode\"\n"
    printf "    sudo reboot\n"
    exit 1
fi

SCRIPT_PATH=`realpath $0`
EVAL_PATH=`dirname $SCRIPT_PATH`
BASE_DIR=`realpath $EVAL_PATH/../..`
BPFKV_PATH="$BASE_DIR/benchmark/BPF-KV"
UTILS_PATH="$BASE_DIR/utils"

MOUNT_POINT="/mnt/sode"
DB_PATH="$MOUNT_POINT/bpfkv_test_db"

DEV_NAME="/dev/nvme0n1"
if [ ! -z $1 ]; then
    DEV_NAME=$1
fi
printf "DEV_NAME=$DEV_NAME\n"

NUM_OPS=1000000
printf "NUM_OPS=$NUM_OPS\n"

# Check whether BPF-KV is built
if [ ! -e "$BPFKV_PATH/simplekv" ]; then
    printf "Cannot find BPF-KV binary. Please build BPF-KV first.\n"
    exit 1
fi

# Disable CPU frequency scaling
$UTILS_PATH/disable_cpu_freq_scaling.sh

# Mount disk
$UTILS_PATH/mount_disk.sh $DEV_NAME $MOUNT_POINT

# Create result folder
mkdir -p $EVAL_PATH/result

# BPF-KV (for xrp and read)
pushd $BPFKV_PATH
for LAYER in 3 6; do
    sudo rm -rf $MOUNT_POINT/*
    printf "Creating a BPF-KV database file with $LAYER layers of index...\n"
    sudo numactl --membind=0 --cpunodebind=0 ./simplekv $DB_PATH $LAYER create

    for NUM_THREADS in {1..12}; do
        printf "Evaluating BPF-KV with $LAYER index lookup, $NUM_THREADS threads, and SODE...\n"
        sudo numactl --membind=0 --cpunodebind=0 ./simplekv $DB_PATH $LAYER get --requests=$NUM_OPS --threads=$NUM_THREADS --use-sode | tee $EVAL_PATH/result/$LAYER-layer-$NUM_THREADS-threads-sode.txt

        printf "Evaluating BPF-KV with $LAYER index lookup, $NUM_THREADS threads, and XRP...\n"
        sudo numactl --membind=0 --cpunodebind=0 ./simplekv $DB_PATH $LAYER get --requests=$NUM_OPS --threads=$NUM_THREADS --use-xrp | tee $EVAL_PATH/result/$LAYER-layer-$NUM_THREADS-threads-xrp.txt

        printf "Evaluating BPF-KV with $LAYER index lookup, $NUM_THREADS threads, and read()...\n"
        sudo numactl --membind=0 --cpunodebind=0 ./simplekv $DB_PATH $LAYER get --requests=$NUM_OPS --threads=$NUM_THREADS | tee $EVAL_PATH/result/$LAYER-layer-$NUM_THREADS-threads-read.txt
    done
done
popd

printf "Done. Results are stored in $EVAL_PATH/result\n"
