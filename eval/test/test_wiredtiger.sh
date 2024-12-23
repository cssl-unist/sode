if [ "$(uname -r)" !=  "5.12.0-sode" ]; then
    printf "Not in SODE kernel. Please run the following commands to boot into SODE kernel:\n"
    printf "    sudo grub-reboot \"Advanced options for Ubuntu>Ubuntu, with Linux 5.12.0-sode\"\n"
    printf "    sudo reboot\n"
    exit 1
fi

SCRIPT_PATH=`realpath $0`
EVAL_PATH=`dirname $SCRIPT_PATH`
BASE_DIR=`realpath $EVAL_PATH/../../`
WT_PATH="$BASE_DIR/benchmark/wiredtiger-sode"
YCSB_PATH="$BASE_DIR/benchmark/My-YCSB"
YCSB_CONFIG_PATH="$YCSB_PATH/wiredtiger/config/test.yaml"
UTILS_PATH="$BASE_DIR/utils"
MOUNT_POINT="/mnt/sode"
DB_PATH="$MOUNT_POINT/tigerhome"

if [ -z $1 ]; then
    printf "Set the device path e.g. /dev/nvme0n1\n"
    exit
fi

DEV_NAME="/dev/nvme0n1"
if [ ! -z $1 ]; then
    DEV_NAME=$1
fi
printf "DEV_NAME=$DEV_NAME\n"

pushd $BASE_DIR/benchmark
./build_and_install_wiredtiger-sode.sh 1> /dev/null 2> /dev/null
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

# Disable CPU frequency scaling
$UTILS_PATH/disable_cpu_freq_scaling.sh

# Mount disk
$UTILS_PATH/mount_disk.sh $DEV_NAME $MOUNT_POINT

cp $YCSB_PATH/wiredtiger/original_config/* $YCSB_PATH/wiredtiger/config

pushd $YCSB_PATH/build

printf "Creating database folder...\n"
sudo rm -rf $MOUNT_POINT/*
sudo mkdir -p $DB_PATH
sed -i 's#data_dir: .*#data_dir: "'$DB_PATH'"#' $YCSB_CONFIG_PATH
export WT_BPF_PATH="$WT_PATH/bpf_prog/wt_bpf.o"

printf "Creating a small WiredTiger database...\n"
sudo -E ./init_wt $YCSB_CONFIG_PATH

printf "Running a short YCSB A experiment with SODE enabled...\n"
sudo -E ./run_wt $YCSB_CONFIG_PATH

popd
printf "Done.\n"
