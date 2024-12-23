import matplotlib
import numpy as np
import csv
import matplotlib.pyplot as plt
import re

files = [
    "fig5_a",
    "fig5_b",
    "fig5_c",
    "fig5_d",
    "fig6_a",
    "fig6_b",
    "fig6_c",
    "fig7_a",
    "fig7_b",
    "fig8_a",
    "fig8_b",
    "fig9",
    "fig10",
    "fig11",
    "fig12",
    "fig13",
    "appendix_fig12"
]

output_path="figure/data/"

#
#   BPF-KV related Figures
#

## fig5_a, fig5_b
## fig6_a, fig6_b

outconfig_list = ["read", "XRP", "SODE"]

config_list = ["read", "xrp", "sode"]
config_dict = {
    "read": "read",
    "xrp": "XRP",
    "sode": "SODE"
}

layer_list = [3, 6]
thread_list = [i for i in range(1, 12 + 1)]

perf_dict = dict()
for config in config_list:
    for layer in layer_list:
        for thread in thread_list:
            with open(f"bpfkv_multi_threads/result/{layer}-layer-{thread}-threads-{config}.txt", "r") as fp:
                data = fp.read()

                perf_dict[(layer, thread, config, "throughput")] = float(re.search("Average throughput: (.*?) op/s", data).group(1))
                perf_dict[(layer, thread, config, "p99_latency")] = float(re.search("99%   latency: (.*?) us", data).group(1))
                perf_dict[(layer, thread, config, "p999_latency")] = float(re.search("99.9% latency: (.*?) us", data).group(1))

                fp.close()

filename = "fig5_a.csv"
output = []
layer = 6
for thread in thread_list:
    data = []
    data.append(thread)
    for config in config_list:
        data.append(perf_dict[(layer, thread, config, "p99_latency")])
    output.append(data)

with open(output_path + filename, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow([''] + outconfig_list)
    for row in output:
        writer.writerow(row)
    f.close()

filename = "fig5_b.csv"
output = []
layer = 6
for thread in thread_list:
    data = []
    data.append(thread)
    for config in config_list:
        data.append(perf_dict[(layer, thread, config, "p999_latency")])
    output.append(data)

with open(output_path + filename, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow([''] + outconfig_list)
    for row in output:
        writer.writerow(row)
    f.close()

filename = "fig6_a.csv"
output = []
layer = 6
for thread in thread_list:
    data = []
    data.append(thread)
    for config in config_list:
        data.append(perf_dict[(layer, thread, config, "throughput")] / 1000)
    output.append(data)

with open(output_path + filename, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow([''] + outconfig_list)
    for row in output:
        writer.writerow(row)
    f.close()

filename = "fig6_b.csv"
output = []
layer = 3
for thread in thread_list:
    data = []
    data.append(thread)
    for config in config_list:
        data.append(perf_dict[(layer, thread, config, "throughput")] / 1000)
    output.append(data)

with open(output_path + filename, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow([''] + outconfig_list)
    for row in output:
        writer.writerow(row)
    f.close()



# fig5_c, fig5_d
# fig6_c


config_list = ["read", "xrp", "sode"]
config_dict = {
    "read": "read",
    "xrp": "XRP",
    "sode": "SODE"
}

layer_list = [1, 2, 3, 4, 5, 6]
thread = 1

perf_dict = dict()
for config in config_list:
    for layer in layer_list:
        with open(f"bpfkv_single_thread/result/{layer}-layer-{config}.txt", "r") as fp:
            data = fp.read()

            perf_dict[(layer, thread, config, "throughput")] = float(re.search("Average throughput: (.*?) op/s", data).group(1))
            perf_dict[(layer, thread, config, "average_latency")] = float(re.search("latency: (.*?) usec", data).group(1))
            perf_dict[(layer, thread, config, "p999_latency")] = float(re.search("99.9% latency: (.*?) us", data).group(1))

            fp.close()

filename = "fig5_c.csv"
output = []
thread = 1
for layer in layer_list:
    data = []
    data.append(layer)
    for config in config_list:
        data.append(perf_dict[(layer, thread, config, "p999_latency")])
    output.append(data)

with open(output_path + filename, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow([''] + outconfig_list)
    for row in output:
        writer.writerow(row)
    f.close()

filename = "fig5_d.csv"
output = []
thread = 1
for layer in layer_list:
    data = []
    data.append(layer)
    for config in config_list:
        data.append(perf_dict[(layer, thread, config, "average_latency")])
    output.append(data)

with open(output_path + filename, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow([''] + outconfig_list)
    for row in output:
        writer.writerow(row)
    f.close()

filename = "fig6_c.csv"
output = []
thread = 1
for layer in layer_list:
    data = []
    data.append(layer)
    for config in config_list:
        data.append(perf_dict[(layer, thread, config, "throughput")] / 1000)
    output.append(data)

with open(output_path + filename, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow([''] + outconfig_list)
    for row in output:
        writer.writerow(row)
    f.close()


# fig7_a, fig7_b

config_list = ["read", "xrp", "sode"]
config_dict = {
    "read": "read",
    "xrp": "XRP",
    "sode": "SODE"
}

range_len_list = [i for i in range(1, 100, 5)]

perf_dict = dict()
for config in config_list:
    for range_len in range_len_list:
        with open(f"bpfkv_range_query/result/{range_len}-range-{config}.txt", "r") as fp:
            data = fp.read()

            perf_dict[(range_len, config, "throughput")] = float(re.search("Average throughput: (.*?) op/s", data).group(1))
            perf_dict[(range_len, config, "average_latency")] = float(re.search("latency: (.*?) usec", data).group(1))

            fp.close()

filename = "fig7_a.csv"
output = []
for range_len in range_len_list:
    data = []
    data.append(range_len)
    for config in config_list:
        data.append(perf_dict[(range_len, config, "average_latency")])
    output.append(data)

with open(output_path + filename, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow([''] + outconfig_list)
    for row in output:
        writer.writerow(row)
    f.close()

filename = "fig7_b.csv"
output = []
for range_len in range_len_list:
    data = []
    data.append(range_len)
    for config in config_list:
        data.append(perf_dict[(range_len, config, "throughput")] / 1000)
    output.append(data)

with open(output_path + filename, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow([''] + outconfig_list)
    for row in output:
        writer.writerow(row)
    f.close()


## fig8_a

outconfig_list = ["XRP", "SODE"]

config_list = ["xrp", "sode"]
config_dict = {
    "xrp": "XRP",
    "sode": "SODE"
}
layer = 6
thread_list = [i for i in range(6, 24 + 1)]

perf_dict = dict()
for config in config_list:
    for thread in thread_list:
        with open(f"bpfkv_thread_scaling/result/{thread}-threads-{config}.txt", "r") as fp:
            data = fp.read()
            perf_dict[(layer, thread, config, "throughput")] = float(re.search("Average throughput: (.*?) op/s", data).group(1))
            fp.close()

filename = "fig8_a.csv"
output = []
layer = 6
for thread in thread_list:
    data = []
    data.append(thread)
    for config in config_list:
        data.append(perf_dict[(layer, thread, config, "throughput")] / 1000)
    output.append(data)

with open(output_path + filename, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow([''] + outconfig_list)
    for row in output:
        writer.writerow(row)
    f.close()


## fig8_b
outconfig_list = [
        "XRP", "XRP avg", "XRP 99", "XRP 99.9",
        "SODE", "SODE avg", "SODE 99", "SODE 99.9",
        "InStorage", "InStorage avg", "InStorage 99", "InStorage 99.9"
]

config_list = ["xrp", "sode"]
config_dict = {
    "xrp": "XRP",
    "sode": "SODE",
}
req_per_sec_list = [60000 * i for i in range(1, 12 + 1)]

perf_dict = dict()
for config in config_list:
    for req_per_sec in req_per_sec_list:
        with open(f"bpfkv_throughput_latency/result/{req_per_sec}-ops-{config}.txt", "r") as fp:
            data = fp.read()
            perf_dict[(req_per_sec, config, "throughput")] = float(re.search("Average throughput: (.*?) op/s", data).group(1))
            perf_dict[(req_per_sec, config, "average_latency")] = float(re.search("latency: (.*?) usec", data).group(1))
            perf_dict[(req_per_sec, config, "p99_latency")] = float(re.search("99%   latency: (.*?) us", data).group(1))
            perf_dict[(req_per_sec, config, "p999_latency")] = float(re.search("99.9% latency: (.*?) us", data).group(1))
            
            fp.close()


filename = "fig8_b.csv"
output = []
for req_per_sec in req_per_sec_list:
    data = []
    for config in config_list:
        data.append(perf_dict[(req_per_sec, config, "throughput")] / 1000)
        data.append(perf_dict[(req_per_sec, config, "average_latency")] / 1000)
        data.append(perf_dict[(req_per_sec, config, "p99_latency")] / 1000)
        data.append(perf_dict[(req_per_sec, config, "p999_latency")] / 1000)

    # For InStorage
    data.append(0)
    data.append(0)
    data.append(0)
    data.append(0)

    output.append(data)

with open(output_path + filename, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(outconfig_list)
    for row in output:
        writer.writerow(row)
    f.close()

#
#   WiredTiger related Figures
#

## fig11
outconfig_list = ["read", "XRP", "SODE"]

plot_zipfian_constant_list = [0.6, 0.7, 0.8, 0.9, 0.99, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6]

config_list = ["read", "xrp", "sode"]

for zipfian in plot_zipfian_constant_list:
    for config in config_list:
        with open(f"wiredtiger_skewness/result/{zipfian}-zipf-{config}.txt", "r") as fp:
            data = fp.read()
            perf_dict[(zipfian, config, "throughput")] = {
                op: float(re.search(f".*overall:.*{op} throughput (.*?) ops/sec", data).group(1))
                for op in ["UPDATE", "INSERT", "READ", "SCAN", "READ_MODIFY_WRITE"]
            }
            fp.close()

file_name = 'fig11.csv'
output = []
for zipfian in plot_zipfian_constant_list:
    data = []
    data.append(zipfian)
    for config in config_list:
        data.append(sum(perf_dict[(zipfian, config, "throughput")].values()))
    output.append(data)

with open(output_path + file_name, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow([''] + outconfig_list)
    for row in output:
        writer.writerow(row)
    f.close()


## fig12

outconfig_list = ["SODE"]
workload_dict = {
    "ycsb_a.yaml": "YCSB A",
    "ycsb_b.yaml": "YCSB B",
    "ycsb_c.yaml": "YCSB C",
    "ycsb_d.yaml": "YCSB D",
    "ycsb_e.yaml": "YCSB E",
    "ycsb_f.yaml": "YCSB F",
}

thread_list = [
    1,
    2,
    3,
]
config_list = ["xrp", "sode"]

perf_dict = dict()

for workload in workload_dict:
    cache_size = "512M"
    for thread in thread_list:
        for config in config_list:
            with open(f"wiredtiger_throughput/result/{workload}-{cache_size}-cache-{thread}-threads-{config}.txt", "r") as fp:
                data = fp.read()
                perf_dict[(workload, cache_size, thread, config, "bpf_io_time")] = float(re.search("bpf_io_time: (.*?)\n", data).group(1))
                perf_dict[(workload, cache_size, thread, config, "bpf_io_count")] = float(re.search("bpf_io_count: (.*?)\n", data).group(1))
                fp.close()


filename = "fig12.csv"
output = []
cachee_size = "512M"

index_row = [
    'A1', 'A2', 'A3',
    'B1', 'B2', 'B3',
    'C1', 'C2', 'C3',
    'D1', 'D2', 'D3',
    'E1', 'E2', 'E3',
    'F1', 'F2', 'F3'
]

i = 0
for workload in workload_dict:
    for thread in thread_list:
        data = []
        data.append(index_row[i])

        if perf_dict[(workload, cache_size, thread, "xrp", "bpf_io_count")] != 0:
            xrp = perf_dict[(workload, cache_size, thread, "xrp", "bpf_io_time")] / perf_dict[(workload, cache_size, thread, "xrp", "bpf_io_count")]
        else:
            xrp = 0
        
        if perf_dict[(workload, cache_size, thread, "sode", "bpf_io_count")] != 0:
            sode = perf_dict[(workload, cache_size, thread, "sode", "bpf_io_time")] / perf_dict[(workload, cache_size, thread, "sode", "bpf_io_count")]
        else:
            sode = 0
        
        if xrp != 0:
            speedup = sode / xrp
        else:
            speedup = 0
            
        data.append(1 - speedup)
        output.append(data)
        i += 1

with open(output_path + filename, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow([''] + outconfig_list)
    for row in output:
        writer.writerow(row)
    f.close()


## fig13

outconfig_list = ["read", "SODE", "XRP", "SODE-single"]
workload_dict = {
    "ycsb_c.yaml": "YCSB C",
}

thread_list = [
    1,
]
config_list = ["read", "sode", "xrp", "sode-noparallel"]

perf_dict = dict()

for workload in workload_dict:
    cache_size = "512M"
    for thread in thread_list:
        for config in config_list:
            with open(f"wiredtiger_throughput/result/{workload}-{cache_size}-cache-{thread}-threads-{config}.txt", "r") as fp:
                data = fp.read()
                perf_dict[(workload, cache_size, thread, config, "throughput")] = {
                    op: float(re.search(f".*overall:.*{op} throughput (.*?) ops/sec", data).group(1))
                    for op in ["UPDATE", "INSERT", "READ", "SCAN", "READ_MODIFY_WRITE"]
                }
                fp.close()

filename = "fig13.csv"
output = []
cachee_size = "512M"
thread = 1
workload = "ycsb_c.yaml"

output.append(0.99)
for config in config_list:
    data = perf_dict[(workload, cache_size, thread, config, "throughput")]
    data = data["READ"]
    output.append(data)

with open(output_path + filename, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow([''] + outconfig_list)
    writer.writerow(output)
    f.close()
