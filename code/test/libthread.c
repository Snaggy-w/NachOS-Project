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


