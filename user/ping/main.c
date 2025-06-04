#include <stdio.h>
#include <string.h>
#include <lib.h>

int main(void)
{
    int sock = socket(1); /* UDP */
    if (sock < 0) {
        printf("socket failed\n");
        return 1;
    }

    const char *msg = "ping";
    sendto(sock, (void*)msg, strlen(msg));
    char buf[16] = {0};
    int n = recvfrom(sock, buf, sizeof(buf));
    if (n > 0) {
        buf[n] = '\0';
        printf("received: %s\n", buf);
    } else {
        printf("no response\n");
    }
    return 0;
}
