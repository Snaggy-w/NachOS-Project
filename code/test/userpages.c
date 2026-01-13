#include "syscall.h"
#include "libthread.h"

#define THIS "aaa"
#define THAT "bbb"
const int N = 10; // Choose it large enough!
void puts(char *s){
	char *p; 
	for (p = s; *p != '\0'; p++) PutChar(*p);
}
void f(void *s){
	int i; 
	for (i = 0; i < N; i++) puts((char *)s);
	int c = GetChar();
	PutChar(c);
}
int main(){
	KDEBUG('t',"started with userprog1\n");
	UserThreadCreate(f, (void *) THIS);
	f((void*) THAT);
	UserThreadJoin(-1);
	Exit(0);
	return 1;
}
