#include "syscall.h"


int main()
{
char ch[20];
GetString(ch, 19);
PutString(ch);
Halt();
}
