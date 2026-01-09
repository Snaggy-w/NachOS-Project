#include "syscall.h"
#include "libthread.h"
sem_t sem;

void worker(void *arg) {
    sem_wait(&sem);
    PutString("In critical section\n");
    sem_post(&sem);
}

int main() {
    sem_init(&sem, 1);

    int i = UserThreadCreate(worker, 0);
    int j = UserThreadCreate(worker, 0);

    UserThreadJoin(i);
    UserThreadJoin(j);

    Halt();
}

