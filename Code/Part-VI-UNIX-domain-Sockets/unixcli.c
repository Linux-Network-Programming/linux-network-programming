#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>

#define SOCKET_PATH "control.socket"

int main(void)
{
    int fd;
    struct sockaddr_un un;
    int size;
    char buf[256];

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    un.sun_family = AF_UNIX;
    strncpy(un.sun_path, SOCKET_PATH, sizeof(un.sun_path) - 1);
    un.sun_path[sizeof(un.sun_path) - 1] = '\0';
    size = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);

    if (connect(fd, (struct sockaddr *)&un, size) < 0) {
        perror("connect");
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to %s\n", SOCKET_PATH);
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        if (write(fd, buf, strlen(buf)) < 0) {
            perror("write");
            break;
        }
    }
    close(fd);
    return 0;
}