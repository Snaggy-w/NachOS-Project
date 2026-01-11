#include "userprocess.h"
#include "system.h"
#include "addrspace.h"
#include "thread.h"
#include "filesys.h"



void StartUserProcess(int arg){
    OpenFile *executable = (OpenFile *)arg;
    
    AddrSpace *space = new AddrSpace(executable);
    // set the address space of this thread as a completely new address space unlike user threads where they share the address space
    currentThread->space = space;

    delete executable;   // being clean by removing the executable object as it is no longer needed after being passed to AddrSpace

    space->InitRegisters();
    space->RestoreState();
    machine->Run();
}


int do_ForkExec(char *executable){

	// attempt to read the executable
	OpenFile *exec = fileSystem->Open(executable);
        if (exec == NULL) {
        	printf("Unable to open file %s\n", executable);
        	return -1;
    	}
	
	Thread *thread = new Thread(executable); // set the thread dbg name as the executable idk...

	thread->Fork(StartUserProcess, (int)exec);
	return thread->tid; // for now return the pid as the tid

}


void do_Exit(int status) {
    AddrSpace *space = currentThread->space;

    // Clean up address space
    delete space;
    currentThread->space = nullptr;

    // Terminate kernel thread
    currentThread->Finish();

    // Never returns
}

