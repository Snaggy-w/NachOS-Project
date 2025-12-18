#include "syscall.h"


int main()
{
char ch = GetChar();
PutChar(ch);
Halt();
}
