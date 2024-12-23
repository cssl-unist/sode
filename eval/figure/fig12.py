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
    'figure.figsize': (3.4,1.2)
})


fig, ax = plt.subplots()

xtics = []
data = {}

for l in labels:
    data[l] = []

labels = ["SODE"]

line_data = []

for entry in dataIn:
    if entry[0] == '':
        continue
    xtics.append(str(entry[0]))
    data[labels[0]].append(float(entry[1]) * 100)


plt.grid(True, axis='y', alpha=1, linestyle='--')
        
colors = ['C1']

bar_width = 0.1

xtics_sub = [i for i in range(0, len(xtics))]
for i, l in enumerate(labels):
    total = data[l]
    
    thread1 = []
    x1 =[]
    thread2 = []
    x2 = []
    thread3 = []
    x3 = []
    
    for j, d in enumerate(total):
        if xtics[j][1] == '1':
            thread1.append(d)
            x1.append(xtics_sub[j] + bar_width*2)
        if xtics[j][1] == '2':
            thread2.append(d)
            x2.append(xtics_sub[j])
        if xtics[j][1] == '3':
            thread3.append(d)
            x3.append(xtics_sub[j] - bar_width*2)
    
    ax.bar(x1, thread1, label='1 Thread', color='C4')
    ax.bar(x2, thread2, label='2 Threads', color='C5')
    ax.bar(x3, thread3, label='3 Threads', color='C6')


#ax2 = ax.twinx()
#ax2.set_ylabel('Resubmission Rate (%)')
#ax2.plot(xtics_sub, line_data, color='c')
#ax2.tick_params(axis='y')
#ax2.set_yticks([0, 25, 50])

ax.legend(loc='upper center',
    handlelength=0.8,
    columnspacing=0.8,
    ncol=4, bbox_to_anchor=(0.5, 1.3), frameon=False)


#plt.ticklabel_format(style='sci', axis='y', scilimits=(0,0))


ax.set_yticks([-2, 0, 2, 4, 6, 8])
#plt.xticks(xtics_sub, ['1', '2', '3', '1', '2', '3', '1', '2', '3', '1', '2', '3', '1', '2', '3', '1', '2', '3'])
ax.set_xticks([(i * 3 + 1) for i in range(0, 6)])
ax.set_xticklabels(['A', 'B', 'C', 'D', 'E', 'F'])

ax.tick_params( axis='x', which='major', direction='in', length=0)
ax.set_axisbelow(True)

ax.set_ylim(-3, 9)
#sec = ax.secondary_xaxis(location=0)
#
#sec.set_xticks([(i * 3 + 1) for i in range(0, 6)], labels=['\nA', '\nB', '\nC', '\nD', '\nE', '\nF'])

sec2 = ax.secondary_xaxis(location=0)
sec2.set_xticks([i*3 - 0.5 for i in range(0, 12)])
sec2.set_xticklabels('')
sec2.tick_params('x', length=10, width=0.75)



#plt.xticks([1, 2, 3, 4, 5, 6])
ax.set_ylabel("Latency Reduction\n(Normalized, %)")
ax.set_xlabel("Workload")


plt.subplots_adjust(
    top = 0.83, bottom = 0.28,
    left = 0.18, right = 0.98
    )

print("done")
plt.savefig('data/{}.pdf'.format(graph_name))
#plt.savefig('data/{}.pgf'.format(graph_name))
