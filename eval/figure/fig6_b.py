import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import math
import sys, os
import matplotlib.ticker as ticker

hatches = list(map(lambda x: x * 5, ['', '/', '+', '\\', '*']))

# CSV format
graph_name = os.path.basename(__file__[:-3])

csv_name = 'data/{}.csv'.format(graph_name)
with open(csv_name,'r') as f:
    dataIn = list(map(lambda x: x.split(','), f.read().split('\n')))
labels = dataIn[0][1:]
dataIn = dataIn[1:]

plt.style.use('seaborn-paper')
matplotlib.use("pgf")

matplotlib.rcParams.update({
    "pgf.texsystem": "pdflatex",
    'font.family': 'serif',
    'font.serif': ['Time New Roman'] + plt.rcParams['font.serif'],
    'font.size'         : 10,
    'text.usetex': True,
    'pgf.rcfonts': False,
    'figure.figsize': (2.3,1.6)
})


xtics = []
data = {}

for l in labels:
    data[l] = []

for entry in dataIn:
    if entry[0] == '':
        continue
    
    xtics.append(int(entry[0]))
    for i in range(0,len(labels)):
        if entry[i+1] == '':
            continue
        data[labels[i]].append(float(entry[i+1]))

colors = ['C0', 'C2', 'C1']
for i, l in enumerate(labels):
    plt.plot(xtics[:len(data[l])], data[l], label=l, marker='o', markersize=3, color=colors[i])
    
plt.legend(loc='upper center',
    #fontsize='medium',
    handlelength=0.8,
    columnspacing=0.8,
    ncol=5, bbox_to_anchor=(0.5, 1.25), frameon=False)

#plt.ticklabel_format(style='sci', axis='y', scilimits=(0,0))

plt.xlabel("Number of Threads")
plt.ylabel("Throughput (kops/sec)")

plt.xticks([1, 3, 5, 7, 9, 11])
plt.ylim(bottom=0)

plt.subplots_adjust(
    top = 0.85, bottom = 0.24,
    left = 0.21, right = 0.98
    )

print("done")
plt.savefig('data/{}.pdf'.format(graph_name))
