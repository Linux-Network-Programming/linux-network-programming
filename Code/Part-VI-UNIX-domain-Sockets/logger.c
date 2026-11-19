#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOCKET_PATH "/tmp/logger.sock"

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_un addr;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s \"message\"\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (sendto(sock, argv[1], strlen(argv[1]), 0, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("sendto");
        close(sock);
        exit(EXIT_FAILURE);
    }
    close(sock);
    return 0;
}