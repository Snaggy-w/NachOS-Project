#include "syscall.h"
#include "libthread.h"

void dumbFunc(void* arg) {
    PutChar(' ');
    UserThreadExit();
}

int main() {
    /* create a 100 threads to see if it is handled correctly */
    for (int i = 0; i < 100; i++) {
        int id = UserThreadCreate(dumbFunc, &i);
        if(id == -1){
            PutString("Not enough memory to allocate user thread stack\n");
            Exit(0);
        }
        PutInt(id);
        PutChar('\n');
    }
    Exit(0);
}
