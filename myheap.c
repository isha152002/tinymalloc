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
#include <sys/mman.h> //for mmap function

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

//----------Global heap instance-----
static struct heapinfo_t heap={0};

//----------init_heap----------------
heap_e init_heap(struct heapinfo_t *h){
    void *start=mmap(NULL, //addr-let kernal choose where to place
                    getpagesize(),//size of memory needed from kernal
                    PROT_READ| PROT_WRITE,  //MEMORY PROTECTION-read n write alllowed to this mem
                    MAP_ANONYMOUS | MAP_PRIVATE, //FLAGS-type of mapping
                    -1,//FD-file to be mapped(here none)
                    0);//starting byte inside file
    
    //error check
    if(start==(void*)-1){ //MAP_FAILED=(void*)-1 LAST ADDR OF KERNAL SPACE
        perror("mmap");
        return HEAP_FAILURE;
    }

    printf("[heap] mmap page at %p\n",start);

    //this start is the first heapchunk_t
    //its usable size= whole page - header size

    struct heapchunk_t *first=(struct heapchunk_t*)start; //
    first->size=getpagesize() - sizeof(struct heapchunk_t);
    first->prevsize=0;
    first->inuse=false;
    first->next=NULL;

    //store global heap descriptor
    h->start=first;
    h->avail=first->size;

    return HEAP_SUCCESS;

}

