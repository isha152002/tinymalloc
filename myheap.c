//Custom heap allocator
// TODO: create macro for heap header size
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

//==========init_heap func===========
//Ask the kernel for one page of memory via mmap, carve out the first (and only) free chunk — the "wilderness" chunk — and wire up the heap descriptor.
//====================================
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

//=========heap_alloc func============
/*Take the first available free chunk, carve out exactly what the user asked for, turn the leftover into a new free chunk, and return a pointer to the to the usable data region (just after the
 heapchunk_t header), exactly like malloc.*/
//=======================================
void * heap_alloc(size_t size){
    //STEP1: align size to 16bytes
    size = (size+15) & ~(size_t)15;//rounds up to the nearest multiple of 16

    //STEP2: check if heap available
    if(size>heap.avail){
        printf("[heap_alloc] nah - requested %zu, only %u available\n",size,heap.avail);
        return (void*)-1;
    }

    //STEP3: store head of free list
    struct heapchunk_t *chunk = heap.start;
    printf("[heap_alloc] found chunk at %p\n",(void*)chunk);

    //STEP:4 slice and dice
    uint32_t old_size=chunk->size;
    chunk->size=(uint32_t)size;
    chunk->inuse=true;

    //STEP5: build the new free chunk after this data chunk
    struct heapchunk_t *next_chunk=(struct heapchunk_t *)((char*)chunk+ sizeof(struct heapchunk_t)+size); //here typecasting to char Because pointer arithmetic in C moves by the size of the type. If i did chunk + 1 on a heapchunk_t * it would jump 24 bytes forward. But i need to jump exactly sizeof(heapchunk_t) + size bytes — so i cast to char * first (1 byte per step), do the arithmetic, then cast back. 

    next_chunk->size = old_size- (uint32_t)size - sizeof(struct heapchunk_t);
    next_chunk->prevsize = (uint32_t)size;// so free() can look back for coalesce
    next_chunk->inuse = false;
    next_chunk->next = chunk->next; //understand when multiple free chunks

    //STEP6: update heap
    heap.start=next_chunk;
    heap.avail -= (uint32_t)size + (uint32_t)sizeof(struct heapchunk_t);

    //STEP7: return pointer to the data region of the chunk
    return (void*)(chunk+1); //+1 skips the header size

}



