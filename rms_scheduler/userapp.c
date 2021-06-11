#include "userapp.h"
#include <time.h>


int main(int argc, char* argv[]){
    unsigned int pid = getpid();
    unsigned int fibo_num = 100000;
    unsigned int job_cycle_bound;
    
    int loop_idx;
    clock_t wakeup_time, process_time, process_start, yield_start;
    printf("from the user, pid: %u\n", pid);
    register_process(pid, argv[1], argv[2]);
   	sscanf(argv[3],"%u", &job_cycle_bound);
    
    printf("PID: %u Number of jobs: %u\n", pid, job_cycle_bound);

    if(!is_pid_registered(pid)){
    	printf("PID %u not written . Most likely it didn't pass admission control.\n", pid);
    	return -1;
    }

	do {
		yield_start = clock();
		yield_process(pid);
		wakeup_time = clock() - yield_start;

		process_start = clock();
	   	do_job(fibo_num);
	   	process_time = clock() - process_start;

	   	printf("PID: %u wakeup: %f, process: %f\n", 
	   		pid,
	   		((float)(wakeup_time))/CLOCKS_PER_SEC, 
	   		((float)(process_time))/CLOCKS_PER_SEC
	   	);

	   	loop_idx++;
	} while(loop_idx < job_cycle_bound);


    deregister_process(pid);
    if(is_pid_registered(pid)){
    	printf("PID not removed. \n");
    	return -1;
    }

    printf("PID gone. \n");
    return 0;
}