from case1 import parse_data

import numpy as np
import matplotlib.pyplot as plt
import glob
import os


if __name__ == '__main__':
	multiprogramming_test_paths = glob.glob('profiler_result_50ms/profile3_*')
	cpu_utilization = np.zeros(len(multiprogramming_test_paths))

	for p in multiprogramming_test_paths:
		fname = os.path.split(p)[1]
		fname = fname.rstrip('.data')
		duration = float(fname.split('_')[-1])
		num_proc = int(fname.split('_')[1])
		curr_data = parse_data(p)
		total_cpu_time = np.cumsum(curr_data[:,-1])[-1]
		#print(20 * curr_data.shape[0], duration)
		print(curr_data.shape[0])
		cpu_utilization[num_proc-1] = total_cpu_time / (50 * curr_data.shape[0])

	plt.figure()
	plt.plot(np.arange(len(cpu_utilization))+1, 100 * cpu_utilization ,marker='o',markersize=5)
	plt.xlabel('# of Processes', fontsize=15)
	plt.ylabel('CPU Utilization (%)', fontsize=15)
	plt.tick_params('both',labelsize=15)
	plt.grid()
	plt.savefig('multiprogramming.png')