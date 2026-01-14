#include "syscall.h"
#include "libthread.h"

#define THAT "X"
#define NUM_THREADS 10
const int N = 2; // Choose it large enough!
void puts(char *s){
	char *p; 
	for (p = s; *p != '\0'; p++) PutChar(*p);
}
void f(void *s){
	int i; 
	for (i = 0; i < N; i++) puts((char *)s);
}
int main(){
	int tids[NUM_THREADS];
	KDEBUG('t',"userprog2 actually started\n");
	for(int k = 0; k < NUM_THREADS; k++){ 
		tids[k] = UserThreadCreate(f, (void*) THAT);
	}
	for(int k = 0; k < NUM_THREADS; k++){ 
		UserThreadJoin(tids[k]);
	}
	char* exit_str = "Waited for all threads of a process to finish!\n";
	f((void*) exit_str);
	Exit(0);
	return 1;
}

