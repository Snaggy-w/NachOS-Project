#include "userthread.h"
#include "thread.h"
#include "system.h"
#include "machine.h"

void StartUserThread(){
    currentThread->space->InitRegisters();
    currentThread->space->RestoreState();

    // TO COMPLETE 
}

int do_UserThreadCreate(int f, int arg) {
    // create thread and execute f with arg
    Thread *newThread = new Thread("new thread");
    newThread->Fork((VoidFunctionPtr)StartUserThread, f);

    // TO COMPLETE 
}
