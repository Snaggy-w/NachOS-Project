#include "syscall.h"


int main() {
	/* just gets a char and prints it */
	int ch = GetChar();
	PutChar(ch);
	Exit(0);
}
