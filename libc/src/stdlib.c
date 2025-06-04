#include <stdlib.h>
#include <lib.h>
#include <stddef.h>
#include <string.h>

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

void *realloc(void *ptr, size_t size)
{
    if (!ptr)
        return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    struct block *b = ((struct block*)ptr) - 1;
    if (b->size >= size)
        return ptr;
    void *n = malloc(size);
    if (!n)
        return NULL;
    memcpy(n, ptr, b->size);
    free(ptr);
    return n;
}

void *calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr)
        memset(ptr, 0, total);
    return ptr;
}

void exit(int status)
{
    (void)status;
    exitu();
}

