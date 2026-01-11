#include "syscall.h"
#include "libthread.h"

mutex_lock_t m;

int main(){

	mutex_init(&m);

	mutex_lock(&m);
	int pid1 = ForkExec("./userpages");
	mutex_unlock(&m);
	PutString("Launched process with pid ");
	PutInt(pid1);
	PutChar('\n');
	mutex_lock(&m);
	int pid2 = ForkExec("./userpages2");
	mutex_unlock(&m);
	PutString("Launched process with pid ");
	PutInt(pid2);
	PutChar('\n');

	//UserThreadJoin(-1);
	//Halt();
	for(;;) Yield();
	return 0;
}
