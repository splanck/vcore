#include "memory.h"
#include "lib.h"
#include "stddef.h"

struct kblock {
    size_t size;
    struct kblock *next;
};

static struct kblock *free_list = NULL;
static unsigned char *cur;
static unsigned char *end;

void init_kheap(void)
{
    cur = kalloc();
    if (cur)
        end = cur + PAGE_SIZE;
}

void *kmalloc(size_t size)
{
    size = (size + 15) & ~15ULL;

    struct kblock **prev = &free_list;
    struct kblock *blk = free_list;
    while (blk) {
        if (blk->size >= size) {
            *prev = blk->next;
            return (void*)(blk + 1);
        }
        prev = &blk->next;
        blk = blk->next;
    }

    if (!cur || cur + sizeof(struct kblock) + size > end) {
        unsigned char *page = kalloc();
        if (!page)
            return NULL;
        cur = page;
        end = cur + PAGE_SIZE;
    }

    struct kblock *block = (struct kblock*)cur;
    block->size = size;
    cur += sizeof(struct kblock) + size;

    return (void*)(block + 1);
}

void kmfree(void *ptr)
{
    if (!ptr)
        return;
    struct kblock *block = ((struct kblock*)ptr) - 1;
    block->next = free_list;
    free_list = block;
}

