#include "syscall.h"

int main()
{
    char ch[20];

    GetString(ch, 19);
    /* test if feof can detect special characters */
    if (feof()) {
        PutString("EOF detected\n");
    } else {
        PutString("No EOF\n");
    }

    PutString(ch);
    Exit(0);
}
