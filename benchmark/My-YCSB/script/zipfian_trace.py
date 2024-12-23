import numpy as np
import sys
from collections import deque


def fnv1_64_hash(key):
    key_bytes = key.to_bytes(length=8, byteorder='little', signed=False)
    hash = 14695981039346656037
    for i in range(8):
        hash *= 1099511628211
        hash ^= key_bytes[i]
    return hash % (2 ** 64)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: <zipfian constant> <output file>")
        exit(1)

    nr_entry = 1073741824
    nr_op = 100000000
    batch_size = 10000
    zipfian_constant = float(sys.argv[1])
    read_ratio = 1
    output_file = sys.argv[2]
    
    cur_nr_op = 0
    cur_nr_iter = 0
    key_list = list()
    raw_key_queue = deque()
    while cur_nr_op < nr_op:
        if len(raw_key_queue) == 0:
            raw_key_arr = np.random.zipf(zipfian_constant, (batch_size,))
            raw_key_queue.extend(raw_key_arr)
        raw_key = int(raw_key_queue.popleft())
        raw_key -= 1
        cur_nr_iter += 1
        if cur_nr_iter % 10000 == 0:
            print(f"{cur_nr_op + 1}/{nr_op} ({((cur_nr_op + 1) / nr_op) * 100:.2f}%)\r", end="")
        if raw_key >= nr_entry:
            continue
        key_list.append(fnv1_64_hash(raw_key) % nr_entry)
        cur_nr_op += 1
    print(f"{nr_op}/{nr_op} (100.00%)")

    with open(output_file, "w") as fp:
        for key in key_list:
            fp.write(f"READ,{key:015}\n")
