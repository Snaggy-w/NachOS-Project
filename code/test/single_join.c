// test/join_single.c
#include "syscall.h"
#include "libthread.h"

void worker(void *arg) {
    PutString("Worker: running\n");
    UserThreadExit();
}

int main() {
    PutString("Main: creating thread\n");
    int tid = UserThreadCreate(worker, 0);

    PutString("Main: joining thread\n");
    UserThreadJoin(tid);

    PutString("Main: joined successfully\n");
    Halt();
}
