#include "syscall.h"



int main(){
	int n = 111222333;
	PutInt(n);
	PutChar('\n');
	Halt();
	return 0;
}
