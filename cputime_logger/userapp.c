#include "userapp.h"

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define COMMAND_CHAR_LENGTH 1000000

//register process id
void register_process(unsigned int pid){
    char command[COMMAND_CHAR_LENGTH];
    memset(command, '\0', COMMAND_CHAR_LENGTH);
    sprintf(command, "echo %u > /proc/mp1/status", pid); 
    system(command);
}


void fibo_sequence(unsigned int number_fibo){
	unsigned long long prev_prev = 1;
	unsigned long long prev = 1;
	unsigned long long curr;
	printf("fibo 0: %llu\n", prev_prev);
	printf("fibo 1: %llu\n", prev);
	for(int i =0;i < number_fibo - 2; i++){
		curr = prev + prev_prev;
		prev_prev = prev;
		prev = curr;
		
		printf("fibo %i: %llu\n", i+2, curr);
	}
}


int main(int argc, char* argv[]){
	unsigned int mypid = getpid();
	register_process(mypid);

	fibo_sequence(10000000);

	return 0;
}
