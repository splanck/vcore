#include <stdio.h>
#include <lib.h>
#include <string.h>
#include <stdlib.h>

int main(void)
{
    char *p = sbrk(4096);
    p[0] = 'A';
    int pid = fork();
    if (pid == 0) {
        printf("child sees %c\n", p[0]);
        p[0] = 'B';
        printf("child wrote\n");
        return 0;
    } else {
        waitu(pid);
        printf("parent sees %c\n", p[0]);
    }
    return 0;
}
