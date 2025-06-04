#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <stddef.h>

void *malloc(size_t size);
void free(void *ptr);
void exit(int status);

#endif
