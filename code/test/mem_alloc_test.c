// test_alloc.c
#include "syscall.h"
#include "mem_alloc.h"
#include "libstd.h"
void test_simple_alloc() {
    //memory_init();
    
    int *p1 = (int *)memory_alloc(sizeof(int) * 10);
    int *p2 = (int *)memory_alloc(sizeof(int) * 20);
    
    // Use the memory
    for (int i = 0; i < 10; i++) {
        p1[i] = i;
    }
    for (int i = 0; i < 10; i++) {
        PutInt(p1[i]);
    }
    
    memory_free(p1);
    memory_free(p2);
}

int main() {
    test_simple_alloc();
    //Halt();
    Exit(0);
}
