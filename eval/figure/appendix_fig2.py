import matplotlib
matplotlib.use('Agg')
#matplotlib.rcParams['pdf.fonttype'] = 42
#matplotlib.rcParams['ps.fonttype'] = 42

hdir = '../wiredtiger_throughput/'

import numpy as np
import matplotlib.pyplot as plt
import re
import matplotlib as mpl

matplotlib.rcParams.update({
    "pgf.texsystem": "pdflatex",
    'font.family': 'serif',
    'font.serif': ['Time New Roman'] + plt.rcParams['font.serif'],
    'font.size'         : 8,
    'text.usetex': True,
    'pgf.rcfonts': False,
})


'''
workload_dict = {
    "ycsb_a.yaml": "YCSB A",
    "ycsb_b.yaml": "YCSB B",
    "ycsb_c.yaml": "YCSB C",
    "ycsb_d.yaml": "YCSB D",
    "ycsb_e.yaml": "YCSB E",
    "ycsb_f.yaml": "YCSB F",
}
'''

workload_dict = {
    "ycsb_a.yaml": "A",
    "ycsb_b.yaml": "B",
    "ycsb_c.yaml": "C",
    "ycsb_d.yaml": "D",
    "ycsb_e.yaml": "E",
    "ycsb_f.yaml": "F",
}
cache_size_dict = {
    "4096M": "4GB",
    "2048M": "2GB",
    "1024M": "1GB",
    "512M": "512MB",
}

thread_list = [
    1,
    2,
    3,
]
config_list = ["read", "xrp", "sode"]

perf_dict = dict()

for workload in workload_dict:
    thread = 1    
    for cache_size in cache_size_dict:
        for config in config_list:
            with open(f"{hdir}/result/{workload}-{cache_size}-cache-{thread}-threads-{config}.txt", "r") as fp:
                data = fp.read()
            perf_dict[(workload, cache_size, thread, config, "average_latency")] = {
                op: float(re.search(f"{op} average latency (.*?) ns", data).group(1))
                for op in ["UPDATE", "INSERT", "READ", "SCAN", "READ_MODIFY_WRITE"]
            }
            perf_dict[(workload, cache_size, thread, config, "p99_latency")] = {
                op: float(re.search(f"{op} p99 latency (.*?) ns", data).group(1))
                for op in ["UPDATE", "INSERT", "READ", "SCAN", "READ_MODIFY_WRITE"]
            }
            perf_dict[(workload, cache_size, thread, config, "throughput")] = {
                op: float(re.search(f".*overall:.*{op} throughput (.*?) ops/sec", data).group(1))
                for op in ["UPDATE", "INSERT", "READ", "SCAN", "READ_MODIFY_WRITE"]
            }

plot_workload_list = ["ycsb_a.yaml", "ycsb_b.yaml", "ycsb_c.yaml", "ycsb_d.yaml", "ycsb_e.yaml", "ycsb_f.yaml"]
plot_cache_size_list = ["512M", "1024M", "2048M", "4096M"]

nr_group = len(plot_workload_list)
nr_bar_per_group = len(plot_cache_size_list) * 3

inter_bar_width = 0.004
bar_width = 0.115

# Create 4 subplots
fig, axs = plt.subplots(1, 4, figsize=(7, 1.5), gridspec_kw={'width_ratios': [3, 1, 1, 1]})


colors = ['C0', 'C2', 'C1']
hatches = ['/////', '....', '\\\\\\\\\\', 'xxx']
mpl.rcParams['hatch.linewidth'] = 0.3 

# Define y-axis limits for each subplot
y_limits = [(0, 150), (0, 270), (0, 25), (0, 70)]

# Define y-axis ticks for each subplot
y_ticks = [
    np.arange(0, 151, 30),  # For subplot 0
    np.arange(0, 271, 50),  # For subplot 1
    np.arange(0, 26, 5),    # For subplot 2
    np.arange(0, 71, 20)    # For subplot 3
]

legend_handles = []

for subplot_index, ax in enumerate(axs):
    if subplot_index == 0:
        X = np.arange(3) * 1.5  # First three groups
        workloads = plot_workload_list[:3]
    else:
        X = np.array([0])  # One group for each of the other subplots
        workloads = [plot_workload_list[subplot_index + 2]]
    
    for config_index, config in enumerate(["read", "xrp", "sode"]):
        for cache_size_index, cache_size in enumerate(plot_cache_size_list):
            value_arr = [sum(perf_dict[(workload, cache_size, 1, config, "throughput")].values()) / 1000
                         for workload in workloads]
            
            bars = ax.bar(X + (inter_bar_width + bar_width) * (3 * cache_size_index + config_index),
                    value_arr, color=colors[config_index], width=bar_width, hatch=hatches[cache_size_index],
                    label=f'{cache_size_dict[cache_size]} Cache ({"Baseline" if config == "read" else "XRP" if config == "xrp" else "SODE"})')
            
            if subplot_index == 0 and config_index == 0:
                legend_handles.append(bars)
            
            for workload_index, workload in enumerate(workloads):
                x = X[workload_index] + (inter_bar_width + bar_width) * (3 * cache_size_index + config_index)
                x -= bar_width / 3 + 0.025
                y = value_arr[workload_index] + 3
                if y >= y_limits[subplot_index][1]:
                    y = y_limits[subplot_index][1] * 0.75
                else:
                    y = y + 1
                if value_arr[workload_index] < 25:
                    y = value_arr[workload_index] + 1
                value = value_arr[workload_index]
                ax.text(x, y, f"{int(value)}", rotation='vertical', fontsize=7,
                         color=colors[config_index] if value < y_limits[subplot_index][1] else 'w', fontweight='bold')

    ax.set_ylim(y_limits[subplot_index])
    ax.set_yticks(y_ticks[subplot_index])
    ax.set_ylabel("Throughput\n(kops/sec)" if subplot_index == 0 else "")
    ax.set_xticks(X + (bar_width) * (nr_bar_per_group / 3))
    ax.set_xticklabels([workload_dict[workload] for workload in workloads])

    ax.tick_params(axis='y', direction='out', length=3, width=1, colors='k',
                    grid_color='k', grid_alpha=0.5)
    ax.tick_params(axis='x', direction='out', length=3, width=0, colors='k',
                    grid_color='k', grid_alpha=0.5)
    
    if subplot_index == 0:
        ax.set_xlim(-0.1, 4.4)
    else:
        ax.set_xlim(-0.1, 1.4)

# Create legend handles and labels
legend_handles = [None] * 12
legend_labels = [None] * 12

for idx, config in enumerate(["Baseline", "XRP", "SODE"]):
    for jdx, cache_size in enumerate(cache_size_dict.values()):
        patch = mpl.patches.Patch(facecolor=colors[["Baseline", "XRP", "SODE"].index(config)], 
                                  hatch=hatches[list(cache_size_dict.values()).index(cache_size)], 
                                  label=f'{config} {cache_size}')
        legend_handles[idx + (jdx) * 3] = patch
        legend_labels[idx + (3-jdx) * 3] = (f'{config} {cache_size}')

# Create a single legend for the entire figure
legend = fig.legend(legend_handles, legend_labels, 
                    ncol=4, loc='upper center', bbox_to_anchor=(0.2, 0.9),
                    columnspacing=1, handlelength=1.5, handletextpad=0.5,
                    fontsize=8, frameon=False)

# Adjust the legend to have three rows
legend._loc = 3  # Upper center
legend._ncol = 4  # 4 columns

plt.tight_layout()
#plt.subplots_adjust(top=0.75, hspace=0.3)  # Adjust to make room for the larger legend at the top
plt.savefig(f"data/appendix_fig2.pdf", format="pdf", bbox_inches="tight", dpi=300)
