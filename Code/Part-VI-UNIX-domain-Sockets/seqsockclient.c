#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>

#define SOCKET_PATH "seqpacket.socket"

#define MSG1 "Hello from client"
#define MSG2 "This is a second message"

int main(void)
{
    int fd;
    struct sockaddr_un un;
    int size;

    fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
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
        exit(EXIT_FAILURE);
    }

    if (send(fd, MSG1, sizeof(MSG1) - 1, 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    if (send(fd, MSG2, sizeof(MSG2) - 1, 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    printf("Two messages sent\n");
    close(fd);
    return 0;
}