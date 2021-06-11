#define COMMAND_CHAR_LENGTH 10000
#define FILE_SYSTEM "/proc/mp2/status"
#define BUFFERLEN 1000

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


void register_process(unsigned int pid, char* period, char* processing_time){
    char command[COMMAND_CHAR_LENGTH];
    memset(command, '\0', COMMAND_CHAR_LENGTH);
    sprintf(command, "echo \"R, %u, %s, %s\" > %s", pid, period, processing_time, FILE_SYSTEM); 
    system(command);
}


void yield_process(unsigned int pid){
	char command[COMMAND_CHAR_LENGTH];
    memset(command, '\0', COMMAND_CHAR_LENGTH);
    sprintf(command, "echo \"Y, %u\" > %s", pid, FILE_SYSTEM); 
    system(command);
}


void deregister_process(unsigned int pid){
	char command[COMMAND_CHAR_LENGTH];
    memset(command, '\0', COMMAND_CHAR_LENGTH);
    sprintf(command, "echo \"D, %u\" > %s", pid, FILE_SYSTEM); 
    system(command);
}


int is_pid_registered(unsigned int pid){
    char filename[50] = FILE_SYSTEM;
    char * schedule_info;
    int bytes_read;
    char * task_node;
    int curr_node_pid;

    schedule_info = malloc(BUFFERLEN * sizeof(char));
    FILE * file;

    file = fopen(filename, "r");
    bytes_read = fread(schedule_info, sizeof(char), BUFFERLEN, file);
    fclose(file);

    task_node = strtok(schedule_info,"\n"); // separate the returned buffer by lines

    while (task_node != NULL){
        sscanf(strsep(&task_node, ":"), "%u", &curr_node_pid); // extract the pid

        if(curr_node_pid == pid){
            return 1;
        } 

        task_node = strtok(NULL, "\n");
    }

    free(schedule_info);
    return 0;
}


void fibo(unsigned int x){
    unsigned int prev_prev = 1;
    unsigned int prev = 1;
    unsigned int curr = 0;
    for(int i = 0;i < x - 2; i++){
        curr = prev + prev_prev;
        prev_prev = prev;
        prev = curr;
    }
}


// calculate x th fibonacci number 100 times
void do_job(unsigned int x){
    for(int i = 0; i < 100; i++){
        fibo(x);
    }
}





