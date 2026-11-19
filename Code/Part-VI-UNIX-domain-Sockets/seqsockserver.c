#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>

#define SOCKET_PATH "seqpacket.socket"

int main(void)
{
    int fd, client_fd;
    struct sockaddr_un un;
    int size;
    char buf[100];
    ssize_t n;

    fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    un.sun_family = AF_UNIX;
    strncpy(un.sun_path, SOCKET_PATH, sizeof(un.sun_path) - 1);

    un.sun_path[sizeof(un.sun_path) - 1] = '\0';
    size = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);

    unlink(SOCKET_PATH);
    if (bind(fd, (struct sockaddr *)&un, size) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(fd, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("SOCK_SEQPACKET server listening on %s\n", SOCKET_PATH);
    client_fd = accept(fd, NULL, NULL);
    if (client_fd < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    buf[n] = '\0';
    printf("Received message 1: %s\n", buf);

    n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    buf[n] = '\0';
    printf("Received message 2: %s\n", buf);
    close(client_fd);
    close(fd);
    unlink(SOCKET_PATH);
    return 0;
}