// test_alloc.c
#include "syscall.h"

void test_simple_alloc() {
    mem_init(4096);
    
    int *p1 = (int *)mem_alloc(sizeof(int) * 10);
    int *p2 = (int *)mem_alloc(sizeof(int) * 20);
    
    // Use the memory
    for (int i = 0; i < 10; i++) {
        p1[i] = i;
    }
    
    mem_free(p1);
    mem_free(p2);
}

int main() {
    test_simple_alloc();
    Exit(0);
}