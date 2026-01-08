#ifndef USERTHREAD_H
#define USERTHREAD_H

#include "thread.h"
#include "system.h"
#include "machine.h"
#include "synch.h"

/* UserThreadBundle
 *	Structure to bundle the properties of the user thread
 *		- f: Address of the user function
 *		- arg: an argument (could be expanded to support multiple args via a list)
 *		- stack: stack pointer
 */
typedef struct UserThreadBundle {
	int entry;
	int f;
	int arg;
	int stack;
} UserThreadBundle_t;

typedef struct threadLockBundle {
	int id;
	Lock *join_lock;
	Semaphore *acquire_sem;
} threadLockBundle_t;

extern void StartUserThread(int f);
extern int do_UserThreadCreate(int entry,int f, int arg);
extern void do_UserThreadExit();
extern void do_UserThreadJoin(int id);
extern void do_Halt();


// Semaphore exports to user space

extern int do_SemInit(int semVal);
extern int do_SemWait(int sem_id);
extern int do_SemPost(int sem_id);
extern int do_SemDestroy(int sem_id);
#endif
