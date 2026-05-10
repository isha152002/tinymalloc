# tinymalloc | Custom heap allocator
My first project in low level programming. Got interested in how memory is allocated in heap using malloc so thought of trying to implement is myself.
A custom heap allocator written in C from scratch. No malloc, no stdlib heap functions, just a raw mmap syscall and everything built on top of it manually.

## What it does

- Asks the Linux kernel directly for memory using `mmap` (one page = 4096 bytes)
- Manages that memory through a free list of chunks, each with a metadata header
- Implements `heap_alloc` — like malloc, returns a pointer to usable memory
- Implements `heap_free` — gives memory back and merges adjacent free chunks
- Handles **fragmentation** through bidirectional coalescing (both forward and backward)

## How it works

Every region of memory is a "chunk". Each chunk has a small header sitting in front of it that tracks:
- how big the data region is
- whether it's currently in use
- the size of the previous chunk (so we can look backwards)
- a pointer to the next free chunk

```
[ heapchunk_t header (24B) | data region ][ heapchunk_t header | data region ] ...
```

When you call `heap_alloc(32)`, it walks the free list to find the first chunk big enough, carves exactly what you asked for out of it, and turns the leftover into a new free chunk. When you call `heap_free`, it gives the memory back and tries to merge with free neighbours on both sides to prevent fragmentation.

## What I learned

- `mmap` as a syscall — how to ask the kernel for raw memory pages
- How malloc-style allocators actually work under the hood (chunk headers, free lists, splitting)
- Why fragmentation happens and how coalescing fixes it
- Pointer arithmetic in C — casting between `void*`, `char*`, and struct pointers

## Building and running

No dependencies, just gcc:

```bash
gcc -Wall -g -o heap heap.c
./heap
```


## Version history

- version 1 :  Basic alloc/free, backward coalescing 
- version 2 : Free list walking (first-fit), split guard 
- version 3 : Forward coalescing, bidirectional merge 


## Resources I read along the way:
1) [mmap] https://www.man7.org/linux/man-pages/man2/mmap.2.html#top_of_page
2) [UserSpace & KernalSpace Concept] https://blogs.oracle.com/linux/userspace-vs-kernelspace-understanding-the-divide
3) [Structural Padding & Alignment] https://www.geeksforgeeks.org/c/structure-member-alignment-padding-and-data-packing/
