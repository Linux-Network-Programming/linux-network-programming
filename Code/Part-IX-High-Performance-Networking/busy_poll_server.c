#include "server.h"

#define SERVER_IP   "10.128.216.118"
#define SERVER_PORT 12346
#define BUSY_POLL_US 50

static void server(int busy_poll_us)
{
    int sockfd;
    struct sockaddr_in addr;
    char buf[BUFSIZE];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_BUSY_POLL, &busy_poll_us, sizeof(busy_poll_us)) == -1) {
        perror("setsockopt SO_BUSY_POLL");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    int value;
    socklen_t optlen = sizeof(value);

    if (getsockopt(sockfd, SOL_SOCKET, SO_BUSY_POLL, &value, &optlen) == -1) {
        perror("getsockopt");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP server listening on %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("SO_BUSY_POLL: %d microseconds\n", value);

    for (;;) {
        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);

        ssize_t nr = recvfrom(sockfd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&client,
                              &client_len);

        if (nr == -1) {
            if (errno == EINTR) continue;
            perror("recvfrom");
            break;
        }

        buf[nr] = '\0';
        printf("Received from %s:%d: %s\n", inet_ntoa(client.sin_addr),
               ntohs(client.sin_port), buf);
    }
    close(sockfd);
}

int main(int argc, char *argv[])
{
    int busy_poll_us = BUSY_POLL_US;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [timeout_us]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 2) {
        char *end;
        long n;

        errno = 0;
        n = strtol(argv[1], &end, 10);
        if (errno != 0 || *argv[1] == '\0' || *end != '\0' || n < 0 || n > INT_MAX) {
            fprintf(stderr, "Invalid timeout\n");
            return EXIT_FAILURE;
        }
        busy_poll_us = (int)n;
    }
    server(busy_poll_us);
    return EXIT_SUCCESS;
}
