#include <stdlib.h>
#include <lib.h>

static unsigned char heap[4096];
static size_t heap_top = 0;

void *malloc(size_t size)
{
    if (heap_top + size > sizeof(heap))
        return 0;
    void *ptr = &heap[heap_top];
    heap_top += size;
    return ptr;
}

void free(void *ptr)
{
    (void)ptr; /* simple allocator does not free */
}

void exit(int status)
{
    (void)status;
    exitu();
}
