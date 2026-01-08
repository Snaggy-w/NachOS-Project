// test/join_many.c
#include "syscall.h"
#include "libthread.h"

#define N 5
#define T 1


void worker(void *arg) {
    int id = *(int *)arg;
    PutString("Thread ");
    PutInt(id);
    PutString(" exiting\n");
}

void worker_creating_threads(void *arg){
    int tids[T];
    int args[T];

    int id = *(int *)arg;
    PutString("Worker thread creator active ");
    PutInt(id);


    for (int i = 0; i < T; i++) {
        args[i] = id+50; //custom to distinct...
        tids[i] = UserThreadCreate(worker, &args[i]);
        if(tids[i] == -1){
            PutString("ERROR, not enough mem to allocate stacks...\n");
            Halt();
        }
    }

    for (int i = 0; i < T; i++) {
        UserThreadJoin(tids[i]);
        PutString("Thread: joined ");
        PutInt(args[i]);
        PutChar('\n');
    }
}

int main() {
    int tids[N];
    int args[N];

    PutString("Main: creating threads\n");

    for (int i = 0; i < N; i++) {
        args[i] = i;
        tids[i] = UserThreadCreate(worker_creating_threads, &args[i]);
        if(tids[i] == -1){
            PutString("ERROR, not enough mem to allocate stacks...\n");
            Halt();
        }
    }

    PutString("Main: joining threads\n");

    for (int i = 0; i < N; i++) {
        UserThreadJoin(tids[i]);
        PutString("Main: joined ");
        PutInt(args[i]);
        PutChar('\n');

    }

    PutString("Main: all threads joined\n");
    Halt();
}
