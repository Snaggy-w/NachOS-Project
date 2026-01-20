#include "syscall.h"
#include "libthread.h"


int main(){
	/* see if we can launch 2 processes on a very low resources system */
	PutString("Started...");
	int pid1 = ForkExec("./userpages");
	PutString("Launched process with pid ");
	PutInt(pid1);
	PutChar('\n');
	int pid2 = -1;
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
