#include "libthread.h"
#include "syscall.h"
// wrappers to make calls posix compliant
int sem_init(sem_t * sem, int val){
	return SemInit((int)sem,val);
}

int sem_wait(sem_t * sem){
	return SemWait(sem->sem_id);
}

int sem_post(sem_t * sem){
	return SemPost(sem->sem_id);
}

int sem_destroy(sem_t * sem){
	return SemDestroy(sem->sem_id);
}

/*
 * Mutex Locks using the semaphores interface
 */

// initialize the lock with semaphore 1
int mutex_init(mutex_lock_t * m){
	return sem_init(&m->lock_sem,1); 
}

int mutex_lock(mutex_lock_t * m){
	return sem_wait(&m->lock_sem);
}

int mutex_unlock(mutex_lock_t * m){
	return sem_post(&m->lock_sem);
}

int mutex_destroy(mutex_lock_t * m){
	return sem_destroy(&m->lock_sem);
}


/*
 * Conditional variables using the semaphores interface
 */

void cond_init(cond_t * c){
	mutex_init(&c->x);
	sem_init(&c->s,0);
	sem_init(&c->h,0);
	c->waiters = 0;
}

void cond_wait(cond_t * c, mutex_lock_t * condition_lock){
	mutex_lock(&c->x); {
		c->waiters++;
	} mutex_unlock(&c->x);
	mutex_unlock(condition_lock); {
		sem_wait(&c->s);
		sem_post(&c->h);
	} mutex_lock(condition_lock);
		
}

void cond_signal(cond_t * c){
	mutex_lock(&c->x);{
		if(c->waiters > 0){
			c->waiters--;
			sem_post(&c->s);
			sem_wait(&c->h);
		}
	} mutex_unlock(&c->x);
}


void cond_broadcast(cond_t * c){
	mutex_lock(&c->x); {
		for(int i = 0 ; i < c->waiters; i++)
			sem_post(&c->s);
		while(c->waiters > 0){
			c->waiters--;
			sem_wait(&c->h);
		}
	} mutex_unlock(&c->x);
}
