#include "userthread.h"
#include "thread.h"
#include "system.h"
#include "machine.h"
#include "synch.h"
#include "list.h"

// UserStackSize: Default 1024 bytes in addrspace.h
// It is the stack being devided by the threads created (not the stack size acquired by newly created threads)
#include "addrspace.h"

// These probably ned to be in the header file

int UthreadID = 0; // A global variable to keep track of user threads created
		   // Might be later good for debugging especially if we use the -d t flag

int userThreadCount = 0; // A counter for threads that are active
Semaphore *userThreadSem = new Semaphore("user thread semaphore",0); // Semaphore to hold the main thread
Semaphore *userThreadLock = new Semaphore("user thread lock",1); // A lock made by a semaphore


// List object to hold threadCondBundle related to each thread

List *ListThreadJoinLock = new List;
Lock *ThreadJoinLock = new Lock("protector for list"); // A lock to protect access to the list in case it is being updated by multiple threads

// A replacement for the POSIX ABI in which we wait all using NULL, here we use -1 since NULL is a pointer

#define USERTHREAD_JOIN_ALL (-1)
/*
 *
 * FindJoinTCB: Helper function that returns a bundle based on the ID
 *
 */
threadLockBundle_t *FindJoinTCB(int id) {
    List *tmp = new List;
    threadLockBundle_t *found = nullptr;
    ThreadJoinLock->Acquire();{
	    // pop out one element at a time, test it -> assign it if found to 'found'
	    while (!ListThreadJoinLock->IsEmpty()) {
		threadLockBundle_t *tcb =
		    (threadLockBundle_t *)ListThreadJoinLock->Remove();

		if (tcb->id == id && found == nullptr) {
		    found = tcb;
		}

		tmp->Append(tcb);
	    }

	    // restore the list from tmp back to the original one
	    while (!tmp->IsEmpty()) {
		ListThreadJoinLock->Append(tmp->Remove());
	    }
    } ThreadJoinLock->Release();
    

    delete tmp;
    return found;
}





// Main required functions

void StartUserThread(int f){
    // cast f to the bundle structure 
    UserThreadBundle *utb = (UserThreadBundle *)f;

    // initialize registers with the AddrSpace object
    currentThread->space->InitRegisters();
    currentThread->space->RestoreState();

    // set the program counter to the function to execute
    machine->WriteRegister(PCReg, utb->entry);
    /* MIPS uses a pipeline as a form of optimization (delay slot execution model)
    *  it tries to execute the next instruction (or at least decode it) while executing the one @ PC
    *  But it doesn't point to the next instruction automatically with PC, so when we change PC, it will still
    *  be OldPC + 4. Thus we need to update it too to try to predict the execution of the next instruction w.r.t 
    *  the function pointed to by PC now
    * We add 4 since MIPS is a 4 byte based instruction set (each instruction is over 4 bytes)
    * ref: https://www.cs.umb.edu/cs641/notes26.html
    */
    machine->WriteRegister(NextPCReg, utb->entry + 4);
    
    // set the argument to the register $a1 which is now the function to execute in userspace
    machine->WriteRegister(4, utb->f);

    // set the actual argument of the function to that is being executed in user space
    machine->WriteRegister(5, utb->arg);

    // set the stack pointer to the stack of the new thread
    machine->WriteRegister(StackReg, utb->stack);

    // finally start the machine execution after updating the machine state
    machine->Run();
}

int do_UserThreadCreate(int entry,int f, int arg) {
    int stackSegmentId = currentThread->space->userStackSegmentMap->Find();
    if (stackSegmentId < 0) { // out of memory
        return -1;
    }
    // increment the thread counter
    userThreadLock->P();
    int tid = ++UthreadID; // UThreadID starts from 0, so we increment then assign
    userThreadCount++;
    userThreadLock->V();

    // String concatination stuff
    char * name = new char[20]; // allocate more to accomodate the number concatinated plus ofc the null terminator
    			        // Gosh I hate dealing with strings
     
    snprintf(name,20,"Thread %d",tid);
    // create thread and execute f with arg
    
    Thread *newThread = new Thread(name);
    
    // We share the address space of the running process with the new thread
    newThread->space = currentThread->space;

    // Stage the bundled structure
    UserThreadBundle *utb = new UserThreadBundle; // apparently that's how we initialize structs in C++
    utb->entry = entry;
    utb->f = f;
    utb->arg = arg;
    // We get the the address where the address space currently is stretched to
    // Then we offset the size of the stack of the new thread (essentially allocating a stack region)
    // The stack of that thread will now grow downwards towards the previous stack (either another thread or main thread)
    // I should probably create a diagram for that...
    /*
    int stackSegmentId = newThread->space->userStackSegmentMap->Find();
    if (stackSegmentId < 0) { // out of memory
        return -1;
    }*/
    newThread->userStackSegmentId = stackSegmentId;
    newThread->tid = tid; // Save the tid in the thread class instance
    utb->stack = currentThread->space->GetNumPages() * PageSize - stackSegmentId * UserStackSegment;

    // create a struct that can be used to hold a thread until it is joined
    threadLockBundle_t *tcb = new threadLockBundle_t {
        .id = tid, 
        .join_lock = new Lock("thread join lock"), 
        .acquire_sem = new Semaphore("acquire lock semaphore", 0)
    };
    ListThreadJoinLock->Append((void *)tcb); // add the structure to the list object
    tcb->join_lock->Acquire(); // acquire the lock so we can lock the thread that tries to join this thread with its ID
    tcb->acquire_sem->V();

    newThread->Fork((VoidFunctionPtr)StartUserThread, (int)utb);

    return tid; // return success
}

/*
 *
 * UserThreadJoin: Hold the calling function until the thread with the specified ID returns or waits for any thread to finish
 * 	if the id is set to USERTHREAD_JOIN_ALL (or -1)
 *
 */
void do_UserThreadJoin(int id){
	if(id == USERTHREAD_JOIN_ALL)
		userThreadSem->P();
	else {
		threadLockBundle_t *tcb = FindJoinTCB(id);
    		if (tcb == nullptr)
        		return; // if something wrong happens 
            tcb->acquire_sem->P();
    		tcb->join_lock->Acquire();
	}
}

/*
 * UserThreadExit: An exit call for a single thread that also tried to release the main thread via a semaphore
 */

void do_UserThreadExit() {
	// get thread lock bundle and unlock the mutex lock to allow the join
	threadLockBundle_t *tcb = FindJoinTCB(currentThread->tid);
	tcb->join_lock->Release();

	userThreadLock->P();
	userThreadCount--;
	if(userThreadCount == 0)
		userThreadSem->V(); // increment the semaphore to make the waiting thread on join(-1) (wait all) continue
	userThreadLock->V();
	
	/* cleanup of the objects: tcb and the lock inside to avoid memory leaks*/
	/*ThreadJoinLock->Acquire();
	ListThreadJoinLock->Remove(tcb);
	ThreadJoinLock->Release();

	delete tcb->join_lock;
	delete tcb;*/

    currentThread->space->userStackSegmentMap->Clear(currentThread->userStackSegmentId);
	currentThread->Finish();
}

void do_Halt(){
	/*userThreadLock->P();
	if(userThreadCount > 0){
		userThreadLock->V();
		userThreadSem->P(); // wait until the last thread releases that semaphore
	} else {
		userThreadLock->V(); 
	}*/

	interrupt->Halt();
}




// Implementation of kernel to user mapped semaphores


typedef struct semEntry{
	int id;
	Semaphore *sem;
	struct semEntry *next;
}semEntry_t;

static Lock *semListLock = new Lock("sem list lock"); // Lock to protect the linked list
semEntry_t semListHead = {
	.id = -1,
	.sem = nullptr,
	.next = nullptr
};
static int nextSemId = 0;


// Linked list to hold semaphores for user space with a dummy head
semEntry_t *semList = &semListHead;

/*
 *
 * Initializes a new kernel Semaphore and returns the identifier of that semaphore
 * Prepend the node to the list
 */
int do_SemInit(int semVal){
    semListLock->Acquire();

    semEntry_t *node = new semEntry_t;
    node->id = nextSemId++;
    node->sem = new Semaphore("user sem", semVal);
    node->next = semList->next;
    semList->next = node;

    semListLock->Release();

    // return the id back to the exception handler
    return node->id;
}

/*
 * A helper to find the the semaphore via the id
 */

Semaphore *FindSemaphore(int id){
    semEntry_t *cur = semList->next;
    while (cur != nullptr) {
        if (cur->id == id)
            return cur->sem;
        cur = cur->next;
    }
    return nullptr;
}



/*
 * a handler to wait on the semaphore
 */

int do_SemWait(int sem_id){
    semListLock->Acquire();
    Semaphore *s = FindSemaphore(sem_id);
    semListLock->Release();

    if (s == nullptr)
        return -1;

    s->P();
    return 0;
}



/*
 * a handler to post on the semaphore
 */

int do_SemPost(int sem_id){
    semListLock->Acquire();
    Semaphore *s = FindSemaphore(sem_id);
    semListLock->Release();

    if (s == nullptr)
        return -1;

    s->V();
    return 0;
}


/*
 * Destroyer for the entry requested
 */

int do_SemDestroy(int sem_id){
    semListLock->Acquire();

    semEntry_t *prev = semList;
    semEntry_t *cur = semList->next;

    while (cur != nullptr) {
        if (cur->id == sem_id) {
            prev->next = cur->next;
            delete cur->sem;
            delete cur;
            semListLock->Release();
            return 0;
        }
        prev = cur;
        cur = cur->next;
    }

    semListLock->Release();
    return -1;
}

