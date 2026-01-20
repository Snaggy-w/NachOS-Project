#include "mem_alloc.h"
#include "liballoc.h"
#include "libthread.h"

//extern int main(void);

// stub to do pre-main setup, for now just setup process heap
void user_init(void){
	mutex_init(&mutex_liballoc);
	memory_init();
}
