// synch.cc
//      Routines for synchronizing threads.  Three kinds of
//      synchronization routines are defined here: semaphores, locks
//      and condition variables (the implementation of the last two
//      are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "synch.h"
#include "copyright.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
//      Initialize a semaphore, so that it can be used for synchronization.
//
//      "debugName" is an arbitrary name, useful for debugging.
//      "initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(const char *debugName, int initialValue) {
    name = debugName;
    value = initialValue;
    queue = new List;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
//      De-allocate semaphore, when no longer needed.  Assume no one
//      is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore() { delete queue; }

//----------------------------------------------------------------------
// Semaphore::P
//      Wait until semaphore value > 0, then decrement.  Checking the
//      value and decrementing must be done atomically, so we
//      need to disable interrupts before checking the value.
//
//      Note that Thread::Sleep assumes that interrupts are disabled
//      when it is called.
//----------------------------------------------------------------------

void Semaphore::P() {
    IntStatus oldLevel = interrupt->SetLevel(IntOff); // disable interrupts
    DEBUG('t',"Waiting on semaphore %s\n",this->name);
    while (value == 0) {                      // semaphore not available
        queue->Append((void *)currentThread); // so go to sleep
        currentThread->Sleep();
    }
    value--; // semaphore available,
    // consume its value

    (void)interrupt->SetLevel(oldLevel); // re-enable interrupts
}

//----------------------------------------------------------------------
// Semaphore::V
//      Increment semaphore value, waking up a waiter if necessary.
//      As with P(), this operation must be atomic, so we need to disable
//      interrupts.  Scheduler::ReadyToRun() assumes that threads
//      are disabled when it is called.
//----------------------------------------------------------------------

void Semaphore::V() {
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    DEBUG('t',"Posting on semaphore %s\n",this->name);
    thread = (Thread *)queue->Remove();
    if (thread != NULL) // make thread ready, consuming the V immediately
        scheduler->ReadyToRun(thread);
    value++;
    (void)interrupt->SetLevel(oldLevel);
}

// Implementation inspired from this paper:
//	- https://birrell.org/andrew/papers/ImplementingCVs.pdf
Lock::Lock(const char *debugName) {
	SemLock = new Semaphore(debugName,1); // initial value set to 1 to make a semaphore act like a lock
}

Lock::~Lock() {
	delete SemLock;
}

void Lock::Acquire() {
	SemLock->P();
}

void Lock::Release() {
	SemLock->V();
}

/*
 *  pthreads implementation also relies on semaphores, see:
 *  	- https://github.com/BrianGladman/pthreads/blob/master/implement.h -> pthread_cond_t_
 *	- https://github.com/BrianGladman/pthreads/blob/master/pthread_cond_wait.c
 *
 *  The implementation is too bloated for our purposes (although probably safer), so we will rely on the simpler
 *  implementation of the paper mentioned above linked again below:
 *	- https://birrell.org/andrew/papers/ImplementingCVs.pdf
 */
Condition::Condition(const char *debugName) {
	this->name = debugName; // probably not the right usage of "name" but it's for debugging
	x = new Lock(name); // Locks and semaphores bare the same name as the condition
	s = new Semaphore(name,0);
	h = new Semaphore(name,0);
	waiters = 0;
}

Condition::~Condition() {
	delete x;
	delete s;
	delete h;
}

void Condition::Wait(Lock *conditionLock) { 
	//ASSERT(FALSE);
	x->Acquire(); { // This scope block thing is sick!
		waiters++;
	} x->Release();
	conditionLock->Release();
	s->P();
	h->V();
	conditionLock->Acquire();
}

void Condition::Signal(Lock *conditionLock) {
	x->Acquire(); {
		if(waiters > 0) {
			waiters--;
			s->V();
			h->P();
		}
	} x->Release();
}

void Condition::Broadcast(Lock *conditionLock) {
	x->Acquire(); {
		for(int i = 0; i < waiters; i++) // unblock all waiters
			s->V();
		while(waiters > 0){ // reset the waiters counter
			waiters--;
			h->P();
		}
	} x->Release();
}
