#include "syscall.h"
#include "libthread.h"

#define THIS "ccc"
#define THAT "ddd"
const int N = 10; // Choose it large enough!
void puts(char *s){
	char *p; 
	for (p = s; *p != '\0'; p++) PutChar(*p);
}
void f(void *s){
	int i; 
	for (i = 0; i < N; i++) puts((char *)s);
}
int main(){
	KDEBUG('t',"userprog2 actually started\n");
	UserThreadCreate(f, (void *) THIS);
	f((void*) THAT);
	UserThreadJoin(-1);
	Exit(0);
	return 1;
}

