#include "syscall.h"
#include "libthread.h"

void dumbFunc(void* arg) {
    //int *i = (int *)arg;
    // PutString("Thread is running...");
    // PutInt(*i);
    PutChar(' ');
    UserThreadExit();
}

int main() {
    for (int i = 0; i < 100; i++) {
        int id = UserThreadCreate(dumbFunc, &i);
        PutInt(id);
        PutChar('\n');
    }
    Halt();
}
