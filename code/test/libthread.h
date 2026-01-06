#ifndef LIBTHREAD_H
#define LIBTHREAD_H
#include "syscall.h"
typedef struct {
    int sem_id;   // kernel-managed semaphore identifier
} sem_t;

typedef struct {
	sem_t lock_sem;	
}mutex_lock_t;


typedef struct {
	mutex_lock_t x;
	sem_t s;
	sem_t h;
	int waiters;
}cond_t;


extern int sem_init(sem_t * sem, int val);
extern int sem_wait(sem_t * sem);
extern int sem_post(sem_t * sem);
extern int sem_destroy(sem_t * sem);

extern int mutex_init(mutex_lock_t * m);
extern int mutex_lock(mutex_lock_t * m);
extern int mutex_unlock(mutex_lock_t * m);
extern int mutex_destroy(mutex_lock_t * m);

extern void cond_init(cond_t * c);
extern void cond_wait(cond_t * c, mutex_lock_t * m);
extern void cond_signal(cond_t * c);
extern void cond_broadcast(cond_t * c);
#endif
