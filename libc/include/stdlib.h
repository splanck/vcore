#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <stddef.h>

void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);
void *calloc(size_t nmemb, size_t size);
long strtol(const char *nptr, char **endptr, int base);
int atoi(const char *nptr);
int abs(int n);
void exit(int status);

#endif
