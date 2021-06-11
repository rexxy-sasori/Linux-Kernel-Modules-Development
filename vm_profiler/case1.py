import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

SAMPLE_INTERVAL = 20
def parse_data(path):
	df = pd.read_csv(path,sep='\n',header=None)
	num_rows = df.shape[0]-1

	count = 0
	plot_data = []
	start = 0
	for idx, row in df.iterrows():
		row = row.values[0].split(' ')
		jiffies, min_fault, maj_fault, cpu_use = float(row[0]), float(row[1]), float(row[2]), float(row[3])
		if idx == 0:
			start = jiffies
		plot_data.append([jiffies-start, min_fault, maj_fault, cpu_use])
		if idx == num_rows-1:
			break

	return np.array(plot_data)


def generate_fig(path, filename):
	plt.figure()
	plot_data = parse_data(path)
	print('total cpu time (ms): ', np.cumsum(plot_data[:,-1])[-1])
	plt.figure()
	plt.plot(SAMPLE_INTERVAL * np.arange(len(plot_data[:,0]))/1000, 
		np.cumsum(plot_data[:,1]+plot_data[:,2]),marker='o',markersize=5
	)
	print(SAMPLE_INTERVAL * np.arange(len(plot_data[:,0]))[-1]/1000)
	plt.xlabel('Time (s)', fontsize=15)
	plt.ylabel('Accumulated Page Fault', fontsize=15)
	plt.tick_params('both',labelsize=15)
	plt.ticklabel_format(axis="y", style="sci", scilimits=(0,0))
	plt.grid()
	plt.savefig(filename)


if __name__ == '__main__':
	generate_fig('profiler_result/profile1.data', 'random.png')
	generate_fig('profiler_result/profile2.data', 'local.png')
	
