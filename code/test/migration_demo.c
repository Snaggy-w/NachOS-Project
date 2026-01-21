#include "syscall.h"

/* a simple program to test process migration */
int g = 41;

int main() {
    PutString("[demo] start\n");

    g += 1;
    PutString("[demo] g=");
    PutInt(g);
    PutChar('\n');

    // put something on stack
    int x = 1234;
    PutString("[demo] x=");
    PutInt(x);
    PutChar('\n');

    PutString("[demo] migrating to machine 1...\n");
    Migrate(1);

    // should run on destination only
    PutString("[demo] after migrate\n");
    PutString("[demo] g=");
    PutInt(g);
    PutChar('\n');

    PutString("[demo] x=");
    PutInt(x);
    PutChar('\n');

    //Exit(0);
    Halt();
}

