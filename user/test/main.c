#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <lib.h>

int main(void)
{
    char buf[64];
    sprintf(buf, "value=%d", 1234);
    printf("%s\n", buf);

    const char *num = "42";
    long v = strtol(num, NULL, 10);
    if (isdigit(num[0]))
        printf("strtol %s -> %d\n", num, v);

    FILE *f = fopen("test.bin", "r");
    if (f) {
        unsigned char header[4];
        size_t n = fread(header, 1, 4, f);
        printf("read %u bytes\n", (unsigned)n);
        fclose(f);
    } else {
        printf("fopen failed\n");
    }

    char *mem = calloc(4, 4);
    mem = realloc(mem, 32);
    strcpy(mem, "done");
    printf("%s\n", mem);
    free(mem);

    return 0;
}
