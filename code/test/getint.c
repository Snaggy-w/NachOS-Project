#include "syscall.h"

int main(){
	/* just gets an int and adds 5 to it */
	int n1 = GetInt();
	PutInt(n1 + 5);
	Exit(0);
}
