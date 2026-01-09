#include "syscall.h"
#include "libthread.h"
void simpleThread(void *arg) {
    PutString("Thread started\n");
    UserThreadExit();
}

int main() {
    PutString("Main: creating threads\n");

    UserThreadCreate(simpleThread, 0);
    UserThreadCreate(simpleThread, 0);
    UserThreadCreate(simpleThread, 0);

    PutString("Main: calling Halt\n");
    Halt();  // must wait for threads

    return 0; // never reached
}

