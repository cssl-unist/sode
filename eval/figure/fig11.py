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
    'figure.figsize': (3,2)
})


plot_zipfian_constant_list = [0.6, 0.7, 0.8, 0.9, 0.99, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6]
data = {}

for l in labels:
    data[l] = []

for entry in dataIn:
    if entry[0] == '':
        continue
    for i in range(0,len(labels)):
        data[labels[i]].append(float(entry[i+1]))

xrp = []
sode = []
for i in range(0, len(data['read'])):
    xrp += [data['XRP'][i] / data['read'][i]]
    sode += [data['SODE'][i] / data['read'][i]]

total = sum(data['XRP']) / sum(data['read'])

plt.axhline(y=29095.15/23485.36, color='C2', linestyle='dashed', label='Uniform (XRP)')
plt.axhline(y=29742.81/23485.36, color='C1', linestyle='dashed', label='Uniform (SODE)')
plt.plot(plot_zipfian_constant_list, xrp, label='Zipfian (XRP)', marker='o', markersize=3, color='C2')
plt.plot(plot_zipfian_constant_list, sode, label='Zipfian (SODE)', marker='o', markersize=3, color='C1')

plt.legend(loc='upper center',
    #fontsize='medium',
    handlelength=0.8,  
    columnspacing=0.8,
    ncol=2, bbox_to_anchor=(0.5, 1.4), frameon=False)

#plt.ticklabel_format(style='sci', axis='y', scilimits=(0,0))
plt.xticks([0.6, 0.8, 1.0, 1.2, 1.4, 1.6])
plt.ylim(0.95, 1.32)
plt.xlabel("Zipfian Constant")
plt.ylabel("Normalized Throughput")
plt.grid(axis='y')


plt.subplots_adjust(
    top = 0.8, bottom = 0.25,
    left = 0.15, right = 0.98
    )

print("done")
plt.savefig('data/{}.pdf'.format(graph_name))

