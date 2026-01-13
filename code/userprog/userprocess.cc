#include "userprocess.h"
#include "system.h"
#include "addrspace.h"
#include "thread.h"
#include "filesys.h"



void StartUserProcess(int arg){
    
    AddrSpace * space = (AddrSpace *)arg;
    currentThread->space = space;
    space->InitRegisters();
    space->RestoreState();
    DEBUG('t',"Before machine->Run()...");
    machine->Run();
}


int do_ForkExec(char *executable){
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	// attempt to read the executable
	OpenFile *exec = fileSystem->Open(executable);
        if (exec == NULL) {
        	printf("Unable to open file %s\n", executable);
		(void)interrupt->SetLevel(oldLevel);
        	return -1;
    	}
	
        AddrSpace *space = new AddrSpace(exec);
	DEBUG('t',"This should be after the creation of an address space...\n");
        delete exec;   // being clean by removing the executable object as it is no longer needed after being passed to AddrSpace
	
	if(space->creation_status == -1){
		//delete space;
		DEBUG('t',"Failed to create process with executable %s\n",executable);
		(void)interrupt->SetLevel(oldLevel);
		return -1;
	}

	Thread *thread = new Thread("New Process"); // set the thread dbg name as the executable idk...
	PIDCounterLock->Acquire(); {
		int pid = ++PIDCounter;
		
		thread->tid = pid;
		process_t *new_process = new process_t {
			.pid = pid,
			.thread = thread,
			.exit_status = PROCESS_RUNNING
		};
		PIDTable->Append((void*)new_process);
	} PIDCounterLock->Release();

	
        // set the address space of this thread as a completely new address space unlike user threads where they share the address space
	thread->Fork(StartUserProcess, (int)space);
	currentThread->space->RestoreState();
	(void)interrupt->SetLevel(oldLevel);
	return thread->tid; // for now return the pid as the tid

}


void do_Exit(int status) {
    AddrSpace *space = currentThread->space;
    //Thread * ref_thread = nullptr;
    int index = 0;
    process_t * proc_struct = nullptr;
    ProcessLoopLock->Acquire(); {
	    for(int i = 0; i < PIDTable->Size(); i++){
		if(((process_t*)(PIDTable->GetAt(i)))->thread == currentThread)
			index = i;
	    }

	    //ref_thread = ((process_t*)(PIDTable->GetAt(index)))->thread;
	    proc_struct = (process_t*)(PIDTable->GetAt(index));
    } ProcessLoopLock->Release();
    DEBUG('t',"Exited PID %d\n",proc_struct->pid); 
    proc_struct->exit_status = status;
    proc_struct->thread = nullptr;
    //delete ref_thread;
     

    // Clean up address space
    delete space;
    currentThread->space = nullptr;

    // Terminate kernel thread
    currentThread->Finish();

    // Never returns
}

