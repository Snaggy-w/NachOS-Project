#include "syscall.h"
#include "libthread.h"
sem_t sem;

void worker(void *arg) {
    sem_wait(&sem);
    PutString("In critical section\n");
    sem_post(&sem);
    UserThreadExit();
}

int main() {
    sem_init(&sem, 1);

    UserThreadCreate(worker, 0);
    UserThreadCreate(worker, 0);

    Halt();
}

