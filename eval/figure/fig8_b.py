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
    'font.size'         : 12,
    'text.usetex': True,
    'pgf.rcfonts': False,
    'figure.figsize': (3.2,2.1)
})


xtics = []
data = {}

#labels = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '10', '11', '12', '13', '14', '15', '16', '17', '18', '19', '20', '21', '22', '23']
labels = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '10', '11']
for l in labels:
    data[l] = []

for entry in dataIn:
    if entry[0] == '':
        continue
    
    for i in range(0,len(labels)):
        if entry[i] == '':
            continue
        data[labels[i]].append(float(entry[i]))


plt.plot(data[labels[8]], data[labels[9]], label="avg (On-Device)", marker='o', markersize=5, color='C4')
plt.plot(data[labels[8]], data[labels[10]], label="99% (On-Device)", marker='s', markersize=5, color='C4')

plt.plot(data[labels[0]], data[labels[1]], label='avg (XRP)', marker='o', markersize=5, color='C2')
plt.plot(data[labels[0]], data[labels[2]], label='99% (XRP)', marker='s', markersize=5, color='C2')

plt.plot(data[labels[4]], data[labels[5]], label="avg (SODE)", marker='o', markersize=5, color='C1')
plt.plot(data[labels[4]], data[labels[6]], label="99% (SODE)", marker='s', markersize=5, color='C1')


plt.axvline(x=max(data[labels[0]]), color='C2', linestyle='--')
plt.axvline(x=max(data[labels[4]]), color='C1', linestyle='--')
plt.axvline(x=max(data[labels[8]]), color='C4', linestyle='--')

plt.text(max(data[labels[0]]) - 15, 35.5, int(max(data[labels[0]])), ha='center', rotation=0, size=10, color='C2', weight='bold')
plt.text(max(data[labels[4]]) + 10, 35.5, int(max(data[labels[4]])), ha='center', rotation=0, size=10, color='C1', weight='bold')
plt.text(max(data[labels[8]]), 35.5, int(max(data[labels[8]])), ha='center', rotation=0, size=10, color='C4', weight='bold')


#plt.plot(data[labels[0]], data[labels[3]], label='99.9% (XRP)', marker='o', markersize=3, color='C7')
#plt.plot(data[labels[4]], data[labels[7]], label="99.9% (ODR)", marker='o', markersize=3, color='C3')
#plt.plot(data[labels[8]], data[labels[11]], label="99.9% (In-Device)", marker='o', markersize=3, color='C4')



#plt.plot(data[labels[8]], data[labels[11]], label='ODR(1+3, sode)', marker='o', markersize=3)
#plt.plot(data[labels[12]], data[labels[15]], label='Prev ODR(1.5+worst)', marker='o', markersize=3)
#plt.plot(data[labels[16]], data[labels[19]], label='ODR(1.5+worst)', marker='o', markersize=3)
#plt.plot(data[labels[20]], data[labels[23]], label='ODR(1.5+worst)', marker='o', markersize=3)

'''
plt.plot(data[labels[0]], data[labels[1]], label='XRP', marker='o', markersize=3)
plt.plot(data[labels[4]], data[labels[5]], label="InStorage(1.5)", marker='o', markersize=3)
plt.plot(data[labels[8]], data[labels[9]], label='ODR(1.5+4ms)', marker='o', markersize=3)
plt.plot(data[labels[12]], data[labels[13]], label='ODR(1.5+32ms)', marker='o', markersize=3)
plt.plot(data[labels[16]], data[labels[17]], label='ODR(1.5+adaptive)', marker='o', markersize=3)
plt.plot(data[labels[20]], data[labels[21]], label='ODR(1.5+worst)', marker='o', markersize=3)
'''

'''
plt.legend(loc='upper center',
    #fontsize='medium',
    handlelength=0.2,
    columnspacing=0.5,
    ncol=3, bbox_to_anchor=(0.43, 1.41), frameon=False)
'''

plt.legend(loc='upper center',
    #fontsize='medium',
    handlelength=0.2,
    columnspacing=0.5,
    ncol=3, bbox_to_anchor=(0.5, 1.45), frameon=False)


#plt.ticklabel_format(style='sci', axis='y', scilimits=(0,0))
#plt.ticklabel_format(style='sci', axis='x', scilimits=(0,0))

plt.xlabel("Throughput (kops/sec)")
plt.ylabel("Latency (ms)")
plt.ylim(bottom=0)

plt.subplots_adjust(
    top = 0.74, bottom = 0.19,
    left = 0.14, right = 0.98
    )

print("done")
plt.savefig('data/{}.pdf'.format(graph_name))

