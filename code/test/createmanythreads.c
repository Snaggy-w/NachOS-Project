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
        if(id == -1){
            PutString("Not enough memory to allocate user thread stack\n");
            Halt();
        }
        PutInt(id);
        PutChar('\n');
    }
    Halt();
}
