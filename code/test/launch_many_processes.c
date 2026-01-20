#include "syscall.h"
#include "libthread.h"

#define NUM_PROCESSES 10
int main(){
	int pid; 
	/* attempt to launch all processes, if failed -> wait until one retunrs and fork */
	for(int k = 0; k < NUM_PROCESSES; k++){
		while((pid = ForkExec("./userprocess_test")) == -1){
			ProcessJoin(PROCESS_JOIN_ANY);
		}
		PutString("Launched process with pid ");
		PutInt(pid);
		PutChar('\n');
	}

	Exit(0);

}
