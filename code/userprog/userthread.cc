#include "userthread.h"
#include "thread.h"
#include "system.h"
#include "machine.h"
#include "synch.h"

// UserStackSize: Default 1024 bytes in addrspace.h
// It is the stack being devided by the threads created (not the stack size acquired by newly created threads)
#include "addrspace.h"

// These probably ned to be in the header file

int UthreadID = 0; // A global variable to keep track of user threads created
		   // Might be later good for debugging especially if we use the -d t flag

int stackOffset = 0; // Each thread will have a stack on top of the previous stack,
		     // so we keep track of these placements with this variable

int userThreadCount = 0; // A counter for threads that are active
Semaphore *userThreadSem = new Semaphore("user thread semaphore",0); // Semaphore to hold the main thread
Semaphore *userThreadLock = new Semaphore("user thread lock",1); // A lock made by a semaphore

void StartUserThread(int f){
    // cast f to the bundle structure 
    UserThreadBundle *utb = (UserThreadBundle *)f;

    // initialize registers with the AddrSpace object
    currentThread->space->InitRegisters();
    currentThread->space->RestoreState();

    // set the program counter to the function to execute
    machine->WriteRegister(PCReg, utb->f);
    /* MIPS uses a pipeline as a form of optimization (delay slot execution model)
    *  it tries to execute the next instruction (or at least decode it) while executing the one @ PC
    *  But it doesn't point to the next instruction automatically with PC, so when we change PC, it will still
    *  be OldPC + 4. Thus we need to update it too to try to predict the execution of the next instruction w.r.t 
    *  the function pointed to by PC now
    * We add 4 since MIPS is a 4 byte based instruction set (each instruction is over 4 bytes)
    * ref: https://www.cs.umb.edu/cs641/notes26.html
    */
    machine->WriteRegister(NextPCReg, utb->f + 4);
    
    // set the argument to the register $a1
    machine->WriteRegister(4, utb->arg);

    // set the stack pointer to the stack of the new thread
    machine->WriteRegister(StackReg, utb->stack);

    // finally start the machine execution after updating the machine state
    machine->Run();
}



int do_UserThreadCreate(int f, int arg) {
    // Thread creation could fail due to insufficient memory

    int newThreadStackSize = PageSize * 2; // For now give each thread two pages as a stack 

    int usedStack = (stackOffset + 1) * newThreadStackSize; // to check if we have space left
    if(usedStack > UserStackSize)
	return -1; // not enough stack space to divide, returns failure

    // increment the thread counter
    userThreadLock->P();
    int tid = UthreadID++;
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
    utb->f = f;
    utb->arg = arg;
    // We get the the address where the address space currently is stretched to
    // Then we offset the size of the stack of the new thread (essentially allocating a stack region)
    // The stack of that thread will now grow downwards towards the previous stack (either another thread or main thread)
    // I should probably create a diagram for that...
    utb->stack = currentThread->space->GetNumPages() * PageSize - (stackOffset + 1) * newThreadStackSize;

    // Increment the offset for other threads
    stackOffset++;

    newThread->Fork((VoidFunctionPtr)StartUserThread, (int)utb);

    return 0; // return success
}


/*
 * UserThreadExit: An exit call for a single thread that also tried to release the main thread via a semaphore
 */

void do_UserThreadExit() {
	userThreadLock->P();
	userThreadCount--;
	if(userThreadCount == 0)
		userThreadSem->V();
	userThreadLock->V();
	currentThread->Finish();
}

void do_Halt(){
	userThreadLock->P();
	if(userThreadCount > 0){
		userThreadLock->V();
		userThreadSem->P(); // wait until the last thread releases that semaphore
	} else {
		userThreadLock->V(); 
	}

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

