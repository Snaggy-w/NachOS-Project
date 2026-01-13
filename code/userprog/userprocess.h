#ifndef USERPROCESS_H
#define USERPROCESS_H

#define PROCESS_RUNNING -2

#include "system.h"
#include "thread.h"
typedef struct process {
	int pid;
	Thread* thread;
	int exit_status;
} process_t;


// forks a kernel thread which should run a process of the specified executable
int do_ForkExec(char* executable);
void do_Exit(int status);
#endif

