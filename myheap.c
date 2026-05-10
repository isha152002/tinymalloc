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
#include <string.h>

#define HEAP_ALIGN 16

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
    size = (size+HEAP_ALIGN-1) & ~(size_t)(HEAP_ALIGN-1);//rounds up to the nearest multiple of 16

    //STEP2: check if heap available
    if(size>heap.avail){
        printf("[heap_alloc] nah - requested %zu, only %u available\n",size,heap.avail);
        return (void*)-1;
    }

    //STEP3: (first-fit)walk the free list to find a chunk big enough
    struct heapchunk_t *chunk = heap.start;
    struct heapchunk_t *prev=NULL;//to patch free list

    while(chunk!=NULL){
        if(chunk->size >=size) break;
        prev=chunk;
        chunk=chunk->next;
    }

    //if no single chunk was large enough(heap is fragmented)
    if(chunk==NULL){
        printf("[heap_alloc] fragmented - total avail=%u but no chunk >=%zu\n",heap.avail,size);
        return (void*)-1;
    }

    printf("[heap_alloc] found chunk at %p (size=%u)\n",(void*)chunk,chunk->size);



    //STEP:4 slice and dice
    uint32_t old_size=chunk->size;
    uint32_t min_split=(uint32_t)sizeof(struct heapchunk_t)+ HEAP_ALIGN;
    chunk->inuse=true;

    if(old_size >= size + min_split){
        chunk->size = (uint32_t)size;

        //STEP5: build the new free chunk after this data chunk
        struct heapchunk_t *left_over=(struct heapchunk_t *)((char*)chunk+ sizeof(struct heapchunk_t)+size); //here typecasting to char Because pointer arithmetic in C moves by the size of the type. If i did chunk + 1 on a heapchunk_t * it would jump 24 bytes forward. But i need to jump exactly sizeof(heapchunk_t) + size bytes — so i cast to char * first (1 byte per step), do the arithmetic, then cast back.

        left_over->size = old_size- (uint32_t)size - sizeof(struct heapchunk_t);
        left_over->prevsize = (uint32_t)size;// so free() can look back for coalesce
        left_over->inuse = false;
        left_over->next = chunk->next; //understand when multiple free chunks

        //patch the free list to remove chunk
        if(prev==NULL){
            //chunk was the head
            heap.start=left_over;
        }
        else{
            //chunk was in middle
            prev->next=left_over;
        }

        heap.avail -= (uint32_t)size + (uint32_t)sizeof(struct heapchunk_t);
        
    }
    else{
        //give whole chunk, just remove it from free list
        chunk->size=old_size;

        if(prev==NULL){
            heap.start=chunk->next;
        }
        else{
            prev->next=chunk->next;
        }

        heap.avail-=old_size;
    }
    

    //STEP7: return pointer to the data region of the chunk
    return (void*)(chunk+1); //+1 skips the header size

}

//==========heap_free==============
/*
Mark the chunk as free. If the previous chunk is also free → coalesce them into one larger free chunk (prevents fragmentation). Otherwise just prepend this chunk to the free list.
*/
//=================================
heap_e heap_free(void* data){ //user gives data region addr
    //STEP1: recover the header addr
    struct heapchunk_t *chunk=&((struct heapchunk_t *)data)[-1]; //Backwards to header position
    printf("[heap_free] freeing chunk at %p,size:%d\n",(void*)chunk,chunk->size);

    //STEP2: look backward for prev chunk
    struct heapchunk_t *prevchunk=NULL;
    if(chunk->prevsize!=NULL){
        printf("[heap_free] prevsize: %d\n", chunk->prevsize);
        prevchunk= (struct heapchunk_t*)((char*)chunk-sizeof(struct heapchunk_t)-chunk->prevsize);
        printf("[heap_free] prev in use? %d\n", prevchunk->inuse);
        printf("[heap_free] prev size %d\n", prevchunk->size);

    }

    if(prevchunk && !prevchunk->inuse){ //prev chunk exists and is not in use
        //COALASCE
        //merge the chunk into the prev free chunk.
        /*
        Before: [ prevchunk | prev_data ][ chunk | data ]
        after: [ prevchunk | prev_data + chunk + data ]
        */
       printf("[heap_free] the chunk behind us is free -coalescing\n");

       prevchunk->size+=sizeof(struct heapchunk_t)+chunk->size;

       //putting merged chunk at the head of free list
       struct heapchunk_t *oldfirst =heap.start;
       heap.start= prevchunk;
       prevchunk->next=oldfirst;

       //only add current chunk's size as the prev one was already added when it was freed
       heap.avail+=chunk->size+(uint32_t)sizeof(struct heapchunk_t);
    }
    else{
        //normal free
        //prepend this chunk to freelist
        struct heapchunk_t *oldfirst=heap.start;
        heap.start=chunk;
        chunk->next=oldfirst;

        heap.avail+=chunk->size;//only add data size as chunk needs its header (meta data)
    }

    chunk->inuse=false;
    return HEAP_SUCCESS;

}

int main(void){
    //********** Test 1 **************
    printf("\n=== test 1: basic alloc/free ===\n");
    heap = (struct heapinfo_t){0};
    init_heap(&heap);
 
    char *s = heap_alloc(32);
    strncpy(s, "hello allocator", 32);
    printf("data = \"%s\"\n", s);
    heap_free(s);
    printf("avail after free: %u\n", heap.avail);

    return 0;
}







