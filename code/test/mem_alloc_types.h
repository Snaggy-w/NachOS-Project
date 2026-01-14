#ifndef   	_MEM_ALLOC_TYPES_H_
#define   	_MEM_ALLOC_TYPES_H_



/* Structure declaration for a free block */
struct memblk_free{
    SizeT size;   
    struct memblk_free* next;
    struct memblk_free* prev;
}; 
typedef struct memblk_free memblk_free_t; 

/* Specific metadata for allocated blocks */
struct memblk_allocated{
    SizeT size;
};
typedef struct memblk_allocated memblk_allocated_t;


#endif
