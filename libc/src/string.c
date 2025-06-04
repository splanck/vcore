#include <string.h>
#include <stddef.h>

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

char *strcpy(char *dst, const char *src)
{
    char *ret = dst;
    while ((*dst++ = *src++))
        ;
    return ret;
}

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c)
            return (char*)s;
        s++;
    }
    if (c == '\0')
        return (char*)s;
    return NULL;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    while (n--) {
        unsigned char c1 = (unsigned char)*s1++;
        unsigned char c2 = (unsigned char)*s2++;
        if (c1 != c2)
            return c1 - c2;
        if (c1 == '\0')
            break;
    }
    return 0;
}
