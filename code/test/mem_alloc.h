
#ifndef   	_MEM_ALLOC_H_
#define   	_MEM_ALLOC_H_

typedef unsigned int SizeT;
#define NULL (void*)0
#define nullptr (void*)0
#include "syscall.h"



/* Allocator functions, to be implemented in mem_alloc.c */
extern void* memory_init(void);
extern void *memory_alloc(SizeT size);
extern void memory_free(void *p);
extern unsigned int memory_get_allocated_block_size(void *addr);


/////////////////////////////////////////////////////////
/* Functions for testing and debugging: */

/* Display function */
void print_mem_state(void); 

/* Function called upon process termination */
void run_at_exit(void);

/* Trace functions: */

/* 
 * This trace function must be called upon a successful block allocation.
 * addr = address of the allocated block payload
 * requested_size = size requested by the user program
 */
void print_alloc_info(void *addr, int requested_size);

/*
 * This trace function must be called upon a block deallocation.
 * addr = address of the allocated block payload
 */
void print_free_info(void *addr);

/*
 * This trace function must be called upon a FAILED block allocation.
 * requested_size = size requested by the user program
 */
void print_alloc_error(int requested_size);


/////////////////////////////////////////////////////////

/* Pointers to the original malloc functions */
// extern void* (*o_malloc)(SizeT);
// extern void (*o_free)(void *);
// extern void* (*o_realloc)(void*, SizeT);
// extern void* (*o_calloc)(SizeT, SizeT);


#endif
