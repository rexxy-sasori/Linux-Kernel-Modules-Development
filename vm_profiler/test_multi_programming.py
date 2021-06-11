import subprocess
import os
import argparse
from datetime import datetime

if __name__ == '__main__':
	# parser = argparse.ArgumentParser()
	# parser.add_argument("num_processes",type=int)
	# args = parser.parse_args()

	check_regi_proc = ['cat', '/proc/mp3/status']
	check_monitor = 'sudo '
	no_regi_proc_ret = ''
	install_module = 'sudo insmod mp3.ko'
	rm_module = 'sudo rmmod mp3'

	for num_proc in range(1, 15):
		# make cmd argument
		os.system(install_module)
		num_processes = num_proc
		single_cmd = ['nice ./work 200 R 10000']
		cmds = num_processes * single_cmd
		cmds = ' & '.join(cmds)

		print(cmds)

		start = datetime.now()
		os.system(cmds)
		
		result = subprocess.check_output(check_regi_proc)
		while result != no_regi_proc_ret:
			result = subprocess.check_output(check_regi_proc)
			if ((datetime.now() - start).total_seconds() > 50):
				break

		end = datetime.now()
		elapse_sec = (end - start).total_seconds()

		print('finish running all processes ... checking data')

		check_monitor = 'sudo ./monitor > profile3_{}_elapse_sec_{}.data'.format(num_processes,elapse_sec)
		os.system(check_monitor)
		print('multi_programming test script done for {}, removing module'.format(num_processes))
		os.system(rm_module)
