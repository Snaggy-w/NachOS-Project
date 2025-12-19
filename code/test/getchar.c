#include "syscall.h"


int main()
{
int ch = GetChar();
PutChar(ch);
Halt();
}
