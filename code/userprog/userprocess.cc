#include "userprocess.h"
#include "system.h"
#include "addrspace.h"
#include "thread.h"
#include "filesys.h"
#include "synch.h"
#include "string.h"


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

	char* threadName = new char[strlen(executable)+1];
	strcpy(threadName, executable);
	Thread *thread = new Thread(threadName); // set the thread dbg name as the executable idk...
	PIDCounterLock->Acquire(); {
		int pid = ++PIDCounter;
		
		thread->tid = pid;
		process_t *new_process = new process_t {
			.pid = pid,
			.thread = thread,
			.exit_status = PROCESS_RUNNING,
			.exited = false,
			.joined = false,
			.PIDLock = new Lock("process join Lock"),
			.PIDCond = new Condition("process join Condition")
		};
		PIDTable->Append((void*)new_process);
	} PIDCounterLock->Release();

	
        // set the address space of this thread as a completely new address space unlike user threads where they share the address space
	thread->Fork(StartUserProcess, (int)space);
	currentThread->space->RestoreState();
	(void)interrupt->SetLevel(oldLevel);
	return thread->tid; // return the tid as the pid (just store it there) 
	
}

int FindAnyJoinableProcess() {
    ProcessLoopLock->Acquire();
    for (int i = 0; i < PIDTable->Size(); i++) {
        process_t *p = (process_t *)PIDTable->GetAt(i);
        if (p->exited && !p->joined) {
            ProcessLoopLock->Release();
            return i;
        }
    }
    ProcessLoopLock->Release();
    return -1;
}

void do_ProcessJoin(int pid){
	
	if(pid == PROCESS_JOIN_ANY){
		/*process_t *p = nullptr;
		int index;
		AnyProcessExitedLock->Acquire();
		while((index = FindAnyJoinableProcess()) == -1)
			AnyProcessExited->Wait(AnyProcessExitedLock);
		p = (process_t *)PIDTable->GetAt(index);
		p->joined = true;
		return;*/

		//SemExitedProcesses->P();
		//return;
		    SemExitedProcesses->P();

	    	// Consume exactly ONE exited process
		    ProcessLoopLock->Acquire();
		    for (int i = 0; i < PIDTable->Size(); i++) {
			process_t *p = (process_t *)PIDTable->GetAt(i);
			if (p->exited && !p->joined) {
			    p->joined = true;   
			    break;
			}
		    }
		    ProcessLoopLock->Release();
		    return;
	}
	
	// Search for PID, if not existent -> abort
	bool found = false;
	int index;
	ProcessLoopLock->Acquire(); {
		for(int i = 0; i < PIDTable->Size(); i++){
			if(((process_t*)(PIDTable->GetAt(i)))->pid == pid){
				found = true;
				index = i;
				break;
			}
		}
		
				
	} ProcessLoopLock->Release();

	if(!found)
		return;

		
	while(((process_t*)(PIDTable->GetAt(index)))->exit_status == PROCESS_RUNNING)
			((process_t*)(PIDTable->GetAt(index)))->PIDCond->Wait(
					((process_t*)(PIDTable->GetAt(index)))->PIDLock
			);

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

    delete space;
    DEBUG('t',"Exited PID %d\n",proc_struct->pid); 
    proc_struct->exit_status = status;
    proc_struct->exited = true;
    proc_struct->joined = false;
    proc_struct->thread = nullptr;
    //delete ref_thread;
    proc_struct->PIDCond->Signal(proc_struct->PIDLock); // release the join    
    AnyProcessExited->Signal(AnyProcessExitedLock);
    SemExitedProcesses->V();
    //delete proc_struct->PIDCond;
    //delete proc_struct->PIDLock;
    // Clean up address space
    currentThread->space = nullptr;

    // Terminate kernel thread
    currentThread->Finish();

    // Never returns
}

