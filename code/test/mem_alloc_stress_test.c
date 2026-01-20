#include "syscall.h"
#include "mem_alloc.h"
#include "libstd.h"

// a test to see the limitations of the heap allocation 
void test_simple_alloc() {
     
    int *p1 = (int *)memory_alloc(sizeof(int) * 10);
    
    // Use the memory
    for (int i = 0; i < 10; i++) {
        p1[i] = i;
    }
    for (int i = 0; i < 10; i++) {
        PutInt(p1[i]);
    }
    

    int *p2 = (int *)memory_alloc(sizeof(int) * 10);
    char *p3 = (char *)memory_alloc(sizeof(char) * (GetProcessHeapSize()/4));
    PutString("allocated huge heap chunk\n");


    for (int i = 0; i < (GetProcessHeapSize()/4); i++) {
        p2[i] = i;
    }
    for (int i = 0; i <(GetProcessHeapSize()/4) ; i++) {
        PutInt(p1[i]);
    }

    memory_free(p2);
    memory_free(p3);	

    /*
    int *p4; // should fail
    if((p4 = (int *)memory_alloc(8000)) == NULL)
	    PutString("failed to alloc memory for p4\n"); 
    */
}

int main() {
    test_simple_alloc();
    //Halt();
    Exit(0);
}
