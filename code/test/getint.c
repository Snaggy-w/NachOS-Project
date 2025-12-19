#include "syscall.h"

int main(){
	int n1 = GetInt();
	PutInt(n1 + 5);
	Halt();
	return 0;
}
