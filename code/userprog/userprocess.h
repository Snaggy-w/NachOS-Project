#ifndef USERPROCESS_H
#define USERPROCESS_H

#define PROCESS_RUNNING -2
#define PROCESS_JOIN_ANY -1

#include "system.h"
#include "thread.h"
#include "synch.h"

typedef struct process {
	int pid;
	Thread* thread;
	int exit_status;
	bool exited;
	bool joined;
	Lock *PIDLock;
	Condition *PIDCond;	
} process_t;


// forks a kernel thread which should run a process of the specified executable
int do_ForkExec(char* executable);
void do_Exit(int status);
void do_ProcessJoin(int pid);


// a bundle for migrated processes that conatins a reference to the newly created 
// address space and the registers to setup
struct MigratedStart {
    AddrSpace* space;
    int regs[NumTotalRegs];
};

// a function to kick-off the migrated process
void StartMigratedUserProcess(int arg);
#endif

