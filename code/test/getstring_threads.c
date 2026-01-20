#include "syscall.h"
#include "libthread.h"
void reader(void *arg) {
    char buf[32];

    PutString("Thread waiting for input:\n");
    GetString(buf, 31);

    PutString("Thread read: ");
    PutString(buf);
    PutChar('\n');

    UserThreadExit();
}

/* create and join two threads that get input and write them out */
int main() {
    PutString("Main: create readers\n");

    int i = UserThreadCreate(reader, 0);
    int j = UserThreadCreate(reader, 0);

    UserThreadJoin(i);
    UserThreadJoin(j);

    Exit(0);
}

