#include "syscall.h"

int main()
{
    char ch[20];

    GetString(ch, 19);

    if (feof()) {
        PutString("EOF detected\n");
    } else {
        PutString("No EOF\n");
    }

    PutString(ch);
    Halt();
}
