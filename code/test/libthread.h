#ifndef LIBTHREAD_H
#define LIBTHREAD_H
#include "syscall.h"
typedef struct {
    int sem_id;   // kernel-managed semaphore identifier
} sem_t;


extern int sem_init(sem_t * sem, int val);
extern int sem_wait(sem_t * sem);
extern int sem_post(sem_t * sem);
extern int sem_destroy(sem_t * sem);
#endif
