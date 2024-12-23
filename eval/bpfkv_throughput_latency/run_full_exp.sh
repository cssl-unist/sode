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

# Specialized BPF-KV
BPFKV_IO_URING_OPEN_LOOP_PATH="$BASE_DIR/benchmark/Specialized-BPF-KV/io_uring_open_loop"
BPFKV_IO_URING_OPEN_LOOP_PATH_SODE="$BASE_DIR/benchmark/Specialized-BPF-KV/io_uring_open_loop_sode"

DEV_NAME="/dev/nvme0n1"
if [ ! -z $1 ]; then
    DEV_NAME=$1
fi
printf "DEV_NAME=$DEV_NAME\n"

NUM_OPS=5000000
LAYER=6
NUM_THREADS=12
printf "NUM_OPS=$NUM_OPS\n"
printf "LAYER=$LAYER\n"
printf "NUM_THREADS=$NUM_THREADS\n"

# Check whether BPF-KV is built
if [ ! -e "$BPFKV_PATH/simplekv" ]; then
    printf "Cannot find BPF-KV binary. Please build BPF-KV first.\n"
    exit 1
fi

# Disable CPU frequency scaling
$UTILS_PATH/disable_cpu_freq_scaling.sh

# Create result folder
mkdir -p $EVAL_PATH/result

# Unmont disk (io_uring is measured with raw block device)
$UTILS_PATH/unmount_disk.sh $DEV_NAME

pushd $BPFKV_IO_URING_OPEN_LOOP_PATH_SODE

# Load database
printf "Creating a BPF-KV database file with $LAYER layers of index...\n"
sudo numactl --membind=0 --cpunodebind=0 ./db-bpf --load $LAYER
for i in {1..12}; do
    REQ_PER_SEC=$((60000 * i))
    printf "Evaluating BPF-KV with $LAYER index lookup, $NUM_THREADS threads, $REQ_PER_SEC ops/s, and SODE...\n"
    # Warmup first
    sudo numactl --membind=0 --cpunodebind=0 ./db-bpf --run $LAYER $NUM_OPS $NUM_THREADS 100 0 0 $(($REQ_PER_SEC / $NUM_THREADS))

    sudo numactl --membind=0 --cpunodebind=0 ./db-bpf --run $LAYER $NUM_OPS $NUM_THREADS 100 0 0 $(($REQ_PER_SEC / $NUM_THREADS)) | tee $EVAL_PATH/result/$REQ_PER_SEC-ops-sode.txt
done
popd

# Specialized BPF-KV for XRP-enabled io_uring with open-loop load generator
pushd $BPFKV_IO_URING_OPEN_LOOP_PATH
# Load database
printf "Creating a BPF-KV database file with $LAYER layers of index...\n"
sudo numactl --membind=0 --cpunodebind=0 ./db-bpf --load $LAYER
for i in {1..12}; do
    REQ_PER_SEC=$((60000 * i))
    printf "Evaluating BPF-KV with $LAYER index lookup, $NUM_THREADS threads, $REQ_PER_SEC ops/s, and XRP...\n"
    # Warmup first
    sudo numactl --membind=0 --cpunodebind=0 ./db-bpf --run $LAYER $NUM_OPS $NUM_THREADS 100 0 0 $(($REQ_PER_SEC / $NUM_THREADS))

    sudo numactl --membind=0 --cpunodebind=0 ./db-bpf --run $LAYER $NUM_OPS $NUM_THREADS 100 0 0 $(($REQ_PER_SEC / $NUM_THREADS)) | tee $EVAL_PATH/result/$REQ_PER_SEC-ops-xrp.txt
done
popd



printf "Done. Results are stored in $EVAL_PATH/result\n"
