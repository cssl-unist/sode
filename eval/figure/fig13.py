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
    'figure.figsize': (3.4,1.0)
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
sode_single = []
for i in range(0, len(data['read'])):
    xrp += [data['XRP'][i] / data['read'][i]]
    sode += [data['SODE'][i] / data['read'][i]]
    sode_single += [data['SODE-single'][i] / data['read'][i]]

bars = plt.bar(['XRP', 'SODE (single)', 'SODE (parallel)'], [xrp[0], sode_single[0], sode[0]], color=['C2', 'C5', 'C1'], width=0.6)

for rect in bars:
    height = rect.get_height()
    plt.text(rect.get_x() + rect.get_width() / 2.0, height*0.35, f'{height:.2f}', ha='center', va='bottom')

#plt.ticklabel_format(style='sci', axis='y', scilimits=(0,0))
#plt.xticks([0.6, 0.8, 1.0, 1.2, 1.4, 1.6])
plt.yticks([0, 0.5, 1, 1.2])
#plt.ylim(0.95, 1.32)
#plt.xlabel("Zipfian Constant")
plt.ylabel("Normalized\nThroughput")


plt.subplots_adjust(
    top = 0.95, bottom = 0.24,
    left = 0.20, right = 0.98
    )

print("done")
plt.savefig('data/{}.pdf'.format(graph_name))
