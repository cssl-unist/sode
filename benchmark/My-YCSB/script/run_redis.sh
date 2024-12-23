if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <number of redis servers>"
    exit 1
fi

WORK_DIR=$(pwd)

# stop redis
while [ -n "$(pgrep redis)" ]
do
    kill $(pgrep redis)
done

# run redis
cd $WORK_DIR/redis
sudo sysctl vm.overcommit_memory=1
sudo bash -c "echo never > /sys/kernel/mm/transparent_hugepage/enabled"
sudo bash -c "echo never > /sys/kernel/mm/transparent_hugepage/defrag"
for i in $(seq 1 $1)
do
    ./src/redis-server ./redis.conf --port $((6379 + $i)) &
done
