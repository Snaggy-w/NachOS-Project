#include "mem_alloc.h"
#include "syscall.h"
#include "mem_alloc_types.h"


#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#define MEM_ALIGNMENT 16
#define MEM_ALLOC_POOL_SIZE 

/* pointer to the beginning of the memory region to manage */
void *heap_begin;

/* Pointer to the first free block in the heap */
memblk_free_t *first_free;

#define ULONG(x) ((long unsigned int)(x))

SizeT round_up(SizeT number) {
    int modulo = number % MEM_ALIGNMENT;
    int quotient = number / MEM_ALIGNMENT;
    return (modulo == 0) ? number : (quotient + 1) * MEM_ALIGNMENT;
}

SizeT aligned_alloc_header_size;
SizeT aligned_free_header_size;


void mem_init_extra() {}

void mem_free_extra(memblk_free_t* new_free_block) {}

void* memory_alloc_impl(SizeT size) {
    memblk_free_t *curr = first_free;

    // the safe size of the allocated block to use
    SizeT aligned_total_payload_size = round_up(aligned_alloc_header_size + size);

    // the block size that we actually need it to fit in the heap (including metadata and alignment padding)
    SizeT safe_size = max(aligned_total_payload_size, aligned_free_header_size);

    //looking for the first fitting free block
    while (curr && safe_size > round_up(curr->size + sizeof(memblk_free_t)))
    {
	//KDEBUG('t',"~~");
        curr = curr->next;
    }

    if (curr == NULL)
    {
        //print_alloc_error(size);
        return NULL;
    }

    memblk_allocated_t *allocated = (memblk_allocated_t*) curr;

    SizeT curr_size = curr->size;
    memblk_free_t* curr_next = curr->next;
    memblk_free_t* curr_prev = curr->prev;

    // have enough space to place the free header at least?
    SizeT total_curr_free_block_size = round_up(curr_size + sizeof(memblk_free_t));
    if (total_curr_free_block_size - safe_size >= aligned_free_header_size)
    {
        allocated->size = safe_size - aligned_alloc_header_size;

        // the start address of the free block header is aligned
        memblk_free_t *new_free = (memblk_free_t *)((char *)(allocated) + safe_size);
        new_free->size = curr_size - safe_size;
        new_free->next = curr_next;
        new_free->prev = curr_prev;

        if (curr_prev) {
            curr_prev->next = new_free;
        } else {
            first_free = new_free;
        }

        if (curr_next != NULL) {
            curr_next->prev = new_free;
        }
    }
    else
    { // Cannot create a free block node out of this free block leftovers
        allocated->size = total_curr_free_block_size - aligned_alloc_header_size;

        // now perform the linking between the neighbouring nodes
        if (curr_prev) {
            curr_prev->next = curr_next;
        } else {
            first_free = curr_next;
        }

        if (curr_next != NULL) {
            curr_next->prev = curr_prev;
        }
    }
    KDEBUG('t',"Done alloc\n");
    void* payload = (void*)((char*)allocated + aligned_alloc_header_size);
    //print_alloc_info(payload, size);

    return payload;
}




void run_at_exit(void)
{
    /* TODO: insert your code here */
}

void* memory_init(void)
{
    /* register the function that will be called when the programs exits */
    //atexit(run_at_exit);

    heap_begin = (void*)Sbrk();
    first_free = heap_begin;
    first_free->size = GetProcessHeapSize() - sizeof(memblk_free_t);
    first_free->next = NULL;
    first_free->prev = NULL;

    aligned_alloc_header_size = round_up(sizeof(memblk_allocated_t));
    aligned_free_header_size = round_up(sizeof(memblk_free_t));

    mem_init_extra();

    return heap_begin;
}


void *memory_alloc(SizeT size)
{
    return memory_alloc_impl(size);
}

void memory_free(void *p)
{
    //print_free_info(p);

    // the beginning address of the payload has to be aligned
    //SizeT aligned_alloc_header_size = round_up(sizeof(memblk_allocated_t));

    memblk_allocated_t* allocated_blk = 
        (memblk_allocated_t*)((char*)p - aligned_alloc_header_size);

    SizeT free_space_size = allocated_blk->size + aligned_alloc_header_size;

    memblk_free_t* new_free_block = (memblk_free_t*) allocated_blk;
    // what has to be aligned here is only the starting address of the free header
    // which is already aligned (because the allocated block header was aligned upom its creation)
    new_free_block->size = free_space_size - sizeof(memblk_free_t);
    new_free_block->prev = NULL;
    new_free_block->next = NULL;

    memblk_free_t* curr = first_free;
    memblk_free_t* prev = NULL;

    // we need to do the linking of the new free block with its neighbours
    while (curr && new_free_block > curr) {
        prev = curr;
        curr = curr->next;
    }

    new_free_block->next = curr;
    new_free_block->prev = prev;

    if (curr) {
        curr->prev = new_free_block;
    }
    
    if (prev) {
        prev->next = new_free_block;
    } else {
        first_free = new_free_block;
    }

    // merge with the next block if possible
    if (curr && (char*)new_free_block + sizeof(memblk_free_t) + new_free_block->size == (char*)curr) {
        new_free_block->size += sizeof(memblk_free_t) + curr->size;
        
        if (curr->next) {
            curr->next->prev = new_free_block;
        }
        new_free_block->next = curr->next;
    }

    // merge with the previous free block if possible
    if (prev && (char*)new_free_block == (char*)prev + sizeof(memblk_free_t) + prev->size) {
        prev->size += new_free_block->size + sizeof(memblk_free_t);
        
        if(new_free_block->next) {
            new_free_block->next->prev = prev;
        }
        prev->next = new_free_block->next;
        new_free_block = prev;
    }

    mem_free_extra(new_free_block);
}

SizeT memory_get_allocated_block_size(void *addr)
{
    memblk_allocated_t* alloc_block = 
        (memblk_allocated_t*)((char*)addr - round_up(sizeof(memblk_allocated_t)));
    return alloc_block->size;
}

// void print(char c, int n) {
//     for (int i = 0; i < n; i++) {
//         fprintf(stderr, "%c", c);
//     }
// }

// void print_mem_state(void)
// {
//     /* TODO: insert your code here */

//     char free_c = '.';
//     char alloc_c = 'X';
//     memblk_free_t* curr_free_blk_start = first_free;
//     void* prev_free_blk_end = heap_begin;
//     void* heap_end = (char*) heap_begin + MEM_POOL_SIZE;

//     while (curr_free_blk_start) {
//         //print out [allocated] space from the previous node end and up to the current node start
//         print(alloc_c, (char*)curr_free_blk_start - (char*)prev_free_blk_end);
//         //print out [free] space ...
//         print(alloc_c, sizeof(memblk_free_t));
//         print(free_c, curr_free_blk_start->size);

//         prev_free_blk_end = (void*)((char*)curr_free_blk_start + sizeof(memblk_free_t) + curr_free_blk_start->size);
//         curr_free_blk_start = curr_free_blk_start->next;
//     }

//     if (prev_free_blk_end != heap_end) {
//         print(alloc_c, (char*)heap_end - (char*)prev_free_blk_end);
//     }
// }

// void print_info(void)
// {
//     fprintf(stderr, "Memory : [%lu %lu] (%lu bytes)\n", (long unsigned int)heap_begin, (long unsigned int)((char *)heap_begin + MEM_POOL_SIZE), (long unsigned int)(MEM_POOL_SIZE));
// }

// void print_free_info(void *addr)
// {
//     //fprintf(stdout, "FREE HEADER SIZE IS %lu \n", ULONG(sizeof(memblk_free_t)));
//     if (addr)
//     {
        
//         fprintf(stderr, "FREE  at : %lu \n", ULONG((char *)addr - (char *)heap_begin));
//     }
//     else
//     {
//         fprintf(stderr, "FREE  at : %lu \n", ULONG(0));
//     }
// }

// void print_alloc_info(void *addr, int requested_size)
// {
//     if (addr)
//     {
//         fprintf(stderr, "ALLOC at : %lu (%d byte(s))\n",
//                 ULONG((char *)addr - (char *)heap_begin), requested_size);
//     }
//     else
//     {
//         fprintf(stderr, "Warning, system is out of memory\n");
//     }
// }

// void print_alloc_error(int requested_size)
// {
//     fprintf(stderr, "ALLOC error : can't allocate %d bytes\n", requested_size);
// }

// int main(int argc, char **argv)
// {

//     /* The main can be changed, it is *not* involved in tests */
//     //memory_init();
//     //print_info();
//     // int i;
//     // for (i = 0; i < 10; i++)
//     // {
//     //     char *b = memory_alloc(rand() % 8);
//     //     memory_free(b);
//     // }

//     // char *a = memory_alloc(15);
//     // memory_free(a);

//     // a = memory_alloc(10);
//     // memory_free(a);

//     //fprintf(stderr, "%lu\n", (long unsigned int)(memory_alloc(9)));
//     return 0;
// }
