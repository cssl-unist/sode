import matplotlib
matplotlib.use('Agg')
#matplotlib.rcParams['pdf.fonttype'] = 42
#matplotlib.rcParams['ps.fonttype'] = 42

hdir = '../wiredtiger_tail_latency/'

import numpy as np
import matplotlib.pyplot as plt
import re

import matplotlib as mpl

matplotlib.rcParams.update({
    "pgf.texsystem": "pdflatex",
    'font.family': 'serif',
    'font.serif': ['Time New Roman'] + plt.rcParams['font.serif'],
    'font.size'         : 9,
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
thread_list = [
    1,
    2,
    3,
]
config_list = ["read", "xrp", "sode"]

perf_dict = dict()

colors = ['C0', 'C2', 'C1']
hatches = ['/////', '....', '\\\\\\\\\\', 'xxx']
mpl.rcParams['hatch.linewidth'] = 0.3 

for workload in workload_dict:
    cache_size = "512M"
    for thread in thread_list:
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
plot_thread_list = [1, 2, 3]

nr_group = len(plot_workload_list)
nr_bar_per_group = len(plot_thread_list) * 3

inter_bar_width = 0.004
bar_width = 0.1

# Create 4 subplots
fig, axs = plt.subplots(1, 4, figsize=(7, 1.2), gridspec_kw={'width_ratios': [3, 1, 1, 1]})



bar_dict = dict()

# Define y-axis limits for each subplot
y_limits = [(0, 150), (0, 480), (0, 35), (0, 80)]
y_ticks = [
    np.arange(0, 151, 30),  # For subplot 0
    np.arange(0, 481, 120),  # For subplot 1
    np.arange(0, 36, 7),  # For subplot 2
    np.arange(0, 81, 20)  # For subplot 3
]

for subplot_index, ax in enumerate(axs):
    if subplot_index == 0:
        X = np.arange(3)  # First three groups
        workloads = plot_workload_list[:3]
    else:
        X = np.array([0])  # One group for each of the other subplots
        workloads = [plot_workload_list[subplot_index + 2]]
    
    for thread_index, thread in enumerate(plot_thread_list):
        color = f"C{thread_index+4}"
        for config_index, config in enumerate(["read", "xrp", "sode"]):
            value_arr = [perf_dict[(workload, "512M", thread, config, "throughput")]["READ" if workload != "ycsb_e.yaml" else "SCAN"] / 1000
                         for workload in workloads]
            
            bars = ax.bar(X + (inter_bar_width + bar_width) * (3 * thread_index + config_index),
                    value_arr, color=colors[config_index], width=bar_width, hatch=hatches[thread_index],
                    label=f'{thread} Thread{" " if thread == 1 else "s"} ({"Baseline" if config == "read" else "XRP"})')
            
            bar_dict[(thread, config)] = bars
            
            for workload_index, workload in enumerate(workloads):
                x = X[workload_index] + (inter_bar_width + bar_width) * (3 * thread_index + config_index)
                x -= bar_width / 3
                y = value_arr[workload_index] + 2
                if y >= y_limits[subplot_index][1]:
                    y = y_limits[subplot_index][1] * 0.75
                else:
                    y = y + 1
                value = value_arr[workload_index]
                ax.text(x, y, f"{int(value)}", rotation='vertical', fontsize=7,
                         color=colors[config_index] if value < y_limits[subplot_index][1] else 'w')

    ax.set_ylim(y_limits[subplot_index])
    ax.set_yticks(y_ticks[subplot_index])
    ax.set_ylabel("Throughput\n(kops/sec)" if subplot_index == 0 else "")
    ax.set_xticks(X + (bar_width) * (nr_bar_per_group / 2.2))
    ax.set_xticklabels([workload_dict[workload] for workload in workloads])

    ax.tick_params(axis='y', direction='out', length=3, width=1, colors='k',
                    grid_color='k', grid_alpha=0.5)
    ax.tick_params(axis='x', direction='out', length=3, width=0, colors='k',
                    grid_color='k', grid_alpha=0.5)
    
legend_handles = []
legend_labels = []

config_map = {"Baseline": "read", "XRP": "xrp", "SODE": "sode"}

for thread in [1, 2, 3]:
    for config in ["Baseline", "XRP", "SODE"]:
        patch = mpl.patches.Patch(
            facecolor=colors[config_list.index(config_map[config])], 
            hatch=hatches[thread - 1],
            label=f'{thread} Thread{"" if thread == 1 else "s"} ({config})'
        )
        legend_handles.append(patch)
        legend_labels.append(f'{thread}T ({config})')

# Add a common legend for all subplots
legend = fig.legend(legend_handles, legend_labels,
                    ncol=9, labelspacing=0.2, columnspacing=0.5, fontsize=6,
                    loc="upper center", bbox_to_anchor=(0.53, 1.1), 
                    handlelength=1.5, handletextpad=0.5, frameon=False)

plt.tight_layout()
plt.subplots_adjust(top=0.88)  # Adjust to make room for the single-line legend
plt.savefig(f"data/fig10.pdf", format="pdf", bbox_inches="tight", dpi=300)
