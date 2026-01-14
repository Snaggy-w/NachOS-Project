#include "syscall.h"
#include "libthread.h"

//mutex_lock_t m;
//cond_t c;

int main(){

	//mutex_init(&m);
	//cond_init(&c);
	PutString("Started...");
	//mutex_lock(&m);
	int pid1 = ForkExec("./userpages");
	//mutex_unlock(&m);
	PutString("Launched process with pid ");
	PutInt(pid1);
	PutChar('\n');
	//mutex_lock(&m);
	int pid2 = -1;
	//mutex_unlock(&m);
	PutString("Launched process with pid ");
	PutInt(pid2);
	PutChar('\n');

	while((pid2 = ForkExec("./userpages2")) == -1)
		ProcessJoin(PROCESS_JOIN_ANY);	
	PutString("Waited for pid 1");
	ProcessJoin(-1);
	PutString("Waited for pid 2");
	Halt();

	//for(;;) Yield();
	return 0;
}
