#include "syscall.h"
#include "libthread.h"
void simpleThread(void *arg) {
    PutString("Thread started\n");
    UserThreadExit();
}

int main() {
    // create 3 threads to see if they fit in a very small stack 
    PutString("Main: creating threads\n");

    UserThreadCreate(simpleThread, 0);
    UserThreadCreate(simpleThread, 0);
    UserThreadCreate(simpleThread, 0);

    PutString("Main: calling Halt\n");
    Exit(0);  
}

