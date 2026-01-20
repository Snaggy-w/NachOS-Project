#include "syscall.h"
#include "libthread.h"

#define N 10 // number of threads to create and join

void worker(void *arg) {
    int id = *(int *)arg;
    PutString("Thread ");
    PutInt(id);
    PutString(" exiting\n");
}

int main() {
    int tids[N];
    int args[N];

    PutString("Main: creating threads\n");

    for (int i = 0; i < N; i++) {
        args[i] = i;
        tids[i] = UserThreadCreate(worker, &args[i]);
    }

    PutString("Main: joining threads\n");

    for (int i = 0; i < N; i++) {
        UserThreadJoin(tids[i]); // join based on tids
        PutString("Main: joined ");
        PutInt(tids[i]);
        PutChar('\n');
    }

    PutString("Main: all threads joined\n");
    Exit(0);
}
