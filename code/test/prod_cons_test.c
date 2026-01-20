#include "libthread.h"
#include "syscall.h"

#define BUFFER_SIZE 5
int buffer[BUFFER_SIZE];
int in = 0;
int out = 0;

// producer consumer test with threads and locking mechanisms/semaphores
sem_t empty;   // counts empty slots
sem_t full;    // counts full slots
sem_t mutex;   // binary semaphore for critical section

void producer(void *arg) {
    int item = 1; // produce items sequentially
    while (item <= 10) { // produce 10 items
        sem_wait(&empty);
        sem_wait(&mutex);

        // critical section
        buffer[in] = item;
        in = (in + 1) % BUFFER_SIZE;

        PutString("Produced item: ");
        PutInt(item);
        PutString("\n");

        sem_post(&mutex);
        sem_post(&full);

        item++;
    }
    UserThreadExit();
}

void consumer(void *arg) {
    int item;
    int count = 0;
    while (count < 10) { // consume 10 items
        sem_wait(&full);
        sem_wait(&mutex);

        // critical section
        item = buffer[out];
        out = (out + 1) % BUFFER_SIZE;

        PutString("Consumed item: ");
        PutInt(item);
        PutString("\n");

        sem_post(&mutex);
        sem_post(&empty);

        count++;
    }
    UserThreadExit();
}

int main() {
    // initialize semaphores
    sem_init(&empty, BUFFER_SIZE); // buffer starts empty
    sem_init(&full, 0);            // no full slots
    sem_init(&mutex, 1);           // mutex unlocked

    // create threads
    UserThreadCreate(producer, 0);
    UserThreadCreate(consumer, 0);

    UserThreadJoin(-1);
    Exit(0); }

