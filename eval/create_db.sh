if [ "$(uname -r)" !=  "5.12.0-sode" ]; then
    printf "Not in SODE kernel. Please run the following commands to boot into SODE kernel:\n"
    printf "    sudo grub-reboot \"Advanced options for Ubuntu>Ubuntu, with Linux 5.12.0-sode\"\n"
    printf "    sudo reboot\n"
    exit 1
fi

SCRIPT_PATH=`realpath $0`
EVAL_PATH=`dirname $SCRIPT_PATH`
BASE_DIR=`realpath $EVAL_PATH/../`

if [ -z $1 ]; then
    printf "Specify the cached DB path\n"
    exit
fi
DB_PATH_BASE=$1

sudo rm -rf $DB_PATH_BASE/*

printf "Generate cached DB: baseline DB first\n"

WT_PATH="$BASE_DIR/benchmark/wiredtiger"
YCSB_PATH="$BASE_DIR/benchmark/My-YCSB"

echo "$BASE_DIR"

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

cp $YCSB_PATH/wiredtiger/original_config/* $YCSB_PATH/wiredtiger/config

for CONFIG in "ycsb_a.yaml" "ycsb_b.yaml" "ycsb_c.yaml" "ycsb_d.yaml" "ycsb_e.yaml" "ycsb_f.yaml"; do

    # DB for tail_latency
    CACHE_SIZE=512
    for NUM_THREADS in 1 2 3; do
        NAME=baseline_tail_${CACHE_SIZE}_${NUM_THREADS}

        DB_PATH=$DB_PATH_BASE/$NAME
        sudo mkdir -p $DB_PATH

        YCSB_CONFIG_PATH="$YCSB_PATH/wiredtiger/config/$CONFIG"

        if [ "$CONFIG" != "ycsb_e.yaml" ]; then
            OP_INTERVAL=50000
        else
            OP_INTERVAL=200000
        fi

        # Update configuration file
        CACHE_SIZE=${CACHE_SIZE}M
        cp $YCSB_PATH/wiredtiger/original_config/* $YCSB_PATH/wiredtiger/config
        sed -i 's#data_dir: .*#data_dir: "'$DB_PATH'"#' $YCSB_CONFIG_PATH
        sed -i 's#nr_thread: .*#nr_thread: '$NUM_THREADS'#' $YCSB_CONFIG_PATH
        sed -i 's#cache_size=[0-9A-Za-z]*,#cache_size='$CACHE_SIZE',#' $YCSB_CONFIG_PATH

        sed -i 's#next_op_interval_ns: [0-9A-Za-z]*#next_op_interval_ns: '$OP_INTERVAL'#' $YCSB_CONFIG_PATH

        # Create database file
        pushd $YCSB_PATH/build
            sudo ./init_wt $YCSB_CONFIG_PATH
        popd
    done

    # DB for throughput
    CACHE_SIZE=512
    for NUM_THREADS in 1 2 3; do
        NAME=baseline_throughput_${CACHE_SIZE}_${NUM_THREADS}

        DB_PATH=$DB_PATH_BASE/$NAME
        sudo mkdir -p $DB_PATH

        YCSB_CONFIG_PATH="$YCSB_PATH/wiredtiger/config/$CONFIG"

        if [ "$CONFIG" != "ycsb_e.yaml" ]; then
            OP_INTERVAL=50000
        else
            OP_INTERVAL=200000
        fi

        # Update configuration file
        CACHE_SIZE=${CACHE_SIZE}M
        cp $YCSB_PATH/wiredtiger/original_config/* $YCSB_PATH/wiredtiger/config
        sed -i 's#data_dir: .*#data_dir: "'$DB_PATH'"#' $YCSB_CONFIG_PATH
        sed -i 's#nr_thread: .*#nr_thread: '$NUM_THREADS'#' $YCSB_CONFIG_PATH
        sed -i 's#cache_size=[0-9A-Za-z]*,#cache_size='$CACHE_SIZE',#' $YCSB_CONFIG_PATH

        # Create database file
        pushd $YCSB_PATH/build
            sudo ./init_wt $YCSB_CONFIG_PATH
        popd
    done

    NUM_THREADS=1
    for CACHE_SIZE in 1024 2048 4096; do
        NAME=baseline_throughput_${CACHE_SIZE}_${NUM_THREADS}

        DB_PATH=$DB_PATH_BASE/$NAME
        sudo mkdir -p $DB_PATH

        YCSB_CONFIG_PATH="$YCSB_PATH/wiredtiger/config/$CONFIG"

        if [ "$CONFIG" != "ycsb_e.yaml" ]; then
            OP_INTERVAL=50000
        else
            OP_INTERVAL=200000
        fi

        # Update configuration file
        CACHE_SIZE=${CACHE_SIZE}M
        cp $YCSB_PATH/wiredtiger/original_config/* $YCSB_PATH/wiredtiger/config
        sed -i 's#data_dir: .*#data_dir: "'$DB_PATH'"#' $YCSB_CONFIG_PATH
        sed -i 's#nr_thread: .*#nr_thread: '$NUM_THREADS'#' $YCSB_CONFIG_PATH
        sed -i 's#cache_size=[0-9A-Za-z]*,#cache_size='$CACHE_SIZE',#' $YCSB_CONFIG_PATH
        #sed -i 's#next_op_interval_ns: [0-9A-Za-z]*#next_op_interval_ns: '$OP_INTERVAL'#' $YCSB_CONFIG_PATH

        # Create database file
        pushd $YCSB_PATH/build
            sudo ./init_wt $YCSB_CONFIG_PATH
        popd
    done
done

printf "Generate cached DB: SODE DB\n"

WT_PATH="$BASE_DIR/benchmark/wiredtiger-sode"
YCSB_PATH="$BASE_DIR/benchmark/My-YCSB"

echo $BASE_DIR
pushd $BASE_DIR/benchmark
./build_and_install_wiredtiger-sode.sh 1> /dev/null 2> /dev/null
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

cp $YCSB_PATH/wiredtiger/original_config/* $YCSB_PATH/wiredtiger/config

for CONFIG in "ycsb_a.yaml" "ycsb_b.yaml" "ycsb_c.yaml" "ycsb_d.yaml" "ycsb_e.yaml" "ycsb_f.yaml"; do
    # DB for tail latency
    CACHE_SIZE=512
    for NUM_THREADS in 1 2 3; do
        NAME=sode_tail_${CACHE_SIZE}_${NUM_THREADS}

        DB_PATH=$DB_PATH_BASE/$NAME
        sudo mkdir -p $DB_PATH

        YCSB_CONFIG_PATH="$YCSB_PATH/wiredtiger/config/$CONFIG"

        if [ "$CONFIG" != "ycsb_e.yaml" ]; then
            OP_INTERVAL=50000
        else
            OP_INTERVAL=200000
        fi

        # Update configuration file
        CACHE_SIZE=${CACHE_SIZE}M
        cp $YCSB_PATH/wiredtiger/original_config/* $YCSB_PATH/wiredtiger/config
        sed -i 's#data_dir: .*#data_dir: "'$DB_PATH'"#' $YCSB_CONFIG_PATH
        sed -i 's#nr_thread: .*#nr_thread: '$NUM_THREADS'#' $YCSB_CONFIG_PATH
        sed -i 's#cache_size=[0-9A-Za-z]*,#cache_size='$CACHE_SIZE',#' $YCSB_CONFIG_PATH

        sed -i 's#next_op_interval_ns: [0-9A-Za-z]*#next_op_interval_ns: '$OP_INTERVAL'#' $YCSB_CONFIG_PATH

        # Create database file
        pushd $YCSB_PATH/build
            sudo ./init_wt $YCSB_CONFIG_PATH
        popd
    done

    # DB for throughput
    CACHE_SIZE=512
    for NUM_THREADS in 1 2 3; do
        NAME=sode_throughput_${CACHE_SIZE}_${NUM_THREADS}

        DB_PATH=$DB_PATH_BASE/$NAME
        sudo mkdir -p $DB_PATH

        YCSB_CONFIG_PATH="$YCSB_PATH/wiredtiger/config/$CONFIG"

        if [ "$CONFIG" != "ycsb_e.yaml" ]; then
            OP_INTERVAL=50000
        else
            OP_INTERVAL=200000
        fi

        # Update configuration file
        CACHE_SIZE=${CACHE_SIZE}M
        cp $YCSB_PATH/wiredtiger/original_config/* $YCSB_PATH/wiredtiger/config
        sed -i 's#data_dir: .*#data_dir: "'$DB_PATH'"#' $YCSB_CONFIG_PATH
        sed -i 's#nr_thread: .*#nr_thread: '$NUM_THREADS'#' $YCSB_CONFIG_PATH
        sed -i 's#cache_size=[0-9A-Za-z]*,#cache_size='$CACHE_SIZE',#' $YCSB_CONFIG_PATH

        # Create database file
        pushd $YCSB_PATH/build
            sudo ./init_wt $YCSB_CONFIG_PATH
        popd
    done

    NUM_THREADS=1
    for CACHE_SIZE in 1024 2048 4096; do
        NAME=sode_${CACHE_SIZE}_${NUM_THREADS}

        DB_PATH=$DB_PATH_BASE/$NAME
        sudo mkdir -p $DB_PATH

        YCSB_CONFIG_PATH="$YCSB_PATH/wiredtiger/config/$CONFIG"

        if [ "$CONFIG" != "ycsb_e.yaml" ]; then
            OP_INTERVAL=50000
        else
            OP_INTERVAL=200000
        fi

        # Update configuration file
        CACHE_SIZE=${CACHE_SIZE}M
        cp $YCSB_PATH/wiredtiger/original_config/* $YCSB_PATH/wiredtiger/config
        sed -i 's#data_dir: .*#data_dir: "'$DB_PATH'"#' $YCSB_CONFIG_PATH
        sed -i 's#nr_thread: .*#nr_thread: '$NUM_THREADS'#' $YCSB_CONFIG_PATH
        sed -i 's#cache_size=[0-9A-Za-z]*,#cache_size='$CACHE_SIZE',#' $YCSB_CONFIG_PATH

        # Create database file
        pushd $YCSB_PATH/build
            sudo ./init_wt $YCSB_CONFIG_PATH
        popd
    done
done

printf "Generate cached DB: SODE-NOPARALLEL DB\n"

WT_PATH="$BASE_DIR/benchmark/wiredtiger-sode-noparallel"
YCSB_PATH="$BASE_DIR/benchmark/My-YCSB"

echo $BASE_DIR
pushd $BASE_DIR/benchmark
./build_and_install_wiredtiger-sode-noparallel.sh 1> /dev/null 2> /dev/null
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

cp $YCSB_PATH/wiredtiger/original_config/* $YCSB_PATH/wiredtiger/config

for CONFIG in "ycsb_c.yaml"; do
    CACHE_SIZE=512
    NUM_THREADS=1
    NAME=sode_noparallel_${CACHE_SIZE}_${NUM_THREADS}

    DB_PATH=$DB_PATH_BASE/$NAME
    sudo mkdir -p $DB_PATH

    YCSB_CONFIG_PATH="$YCSB_PATH/wiredtiger/config/$CONFIG"

    if [ "$CONFIG" != "ycsb_e.yaml" ]; then
        OP_INTERVAL=50000
    else
        OP_INTERVAL=200000
    fi

    # Update configuration file
    CACHE_SIZE=${CACHE_SIZE}M
    cp $YCSB_PATH/wiredtiger/original_config/* $YCSB_PATH/wiredtiger/config
    sed -i 's#data_dir: .*#data_dir: "'$DB_PATH'"#' $YCSB_CONFIG_PATH
    sed -i 's#nr_thread: .*#nr_thread: '$NUM_THREADS'#' $YCSB_CONFIG_PATH
    sed -i 's#cache_size=[0-9A-Za-z]*,#cache_size='$CACHE_SIZE',#' $YCSB_CONFIG_PATH

    # Create database file
    pushd $YCSB_PATH/build
        sudo ./init_wt $YCSB_CONFIG_PATH
    popd
done
