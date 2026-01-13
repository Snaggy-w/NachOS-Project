#ifndef MEM_ALLOC_H
#define MEM_ALLOC_H

#include "system.h"
#include "synch.h"

struct memblk_free{
    size_t size; // corresponds full size fo the block excluding the header 
    /* TODO: DEFINE */
    struct memblk_free *next;
    // header_free 
}; 

typedef struct memblk_free memblk_free_t; 

/* Specific metadata for allocated blocks */
struct memblk_allocated{
    size_t size; 
    /* TODO: DEFINE */
};

typedef struct memblk_allocated memblk_allocated_t;

/* Allocator functions, to be implemented in mem_alloc.c */
void memory_init(size_t size);
void *memory_alloc(size_t size);
void memory_free(void *p);
size_t memory_get_allocated_block_size(void *addr);


#endif

