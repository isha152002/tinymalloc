//Custom heap allocator
/*

* Heap layout (one page from mmap):
 [ heapchunk_t | data ][ heapchunk_t | data ][ ... ]

* heapinfo_t tracks the start of the free list.
* heapchunk_t is the metadata header placed before every chunk of data.
* The free list is a singly-linked list of free chunks (chunk->next).
*/

#include <stdio.h>
#include <stdint.h> //defines fixed-width integer types
#include <stdbool.h> //boolean logic

//------------Return type----------
typedef enum{
    HEAP_SUCCESS=0,
    HEAP_FAILURE=-1,
} heap_e;


//------------Structs-------------

//metadata of each chunk stored before every chunk
struct heapchunk_t{
    uint32_t size; //usable bytes after this header
    uint32_t prevsize;// size of prev chunk 
    bool inuse;// true=allocated, false=free
    struct heapchunk_t *next;// next free chunk(free list only)
};

//Global heap descriptor
struct heapinfo_t{
    struct heapchunk_t *start; //points to the head of free list
    uint32_t avail;// tracks total free bytes remaining
};

//----------init_heap----------------
heap_e init_heap(struct heapinfo_t *h){
    
}

