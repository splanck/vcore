#include <stdlib.h>
#include <lib.h>
#include <stddef.h>

struct block {
    size_t size;
    struct block *next;
};

static struct block *free_list = 0;

void *malloc(size_t size)
{
    size = (size + 15) & ~((size_t)15);
    struct block **prev = &free_list;
    struct block *b = free_list;
    while (b) {
        if (b->size >= size) {
            *prev = b->next;
            return (void*)(b + 1);
        }
        prev = &b->next;
        b = b->next;
    }
    struct block *nb = (struct block*)sbrk(size + sizeof(struct block));
    if (nb == (void*)-1 || nb == NULL)
        return NULL;
    nb->size = size;
    return (void*)(nb + 1);
}

void free(void *ptr)
{
    if (!ptr)
        return;
    struct block *b = ((struct block*)ptr) - 1;
    b->next = free_list;
    free_list = b;
}

void exit(int status)
{
    (void)status;
    exitu();
}

