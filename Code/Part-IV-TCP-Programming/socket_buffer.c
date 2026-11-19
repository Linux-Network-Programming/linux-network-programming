
#include "server.h"

#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 12346
#define RECV_BUF_SIZE 12

int main(void)
{
    int listenfd, connfd;
    int rcvbuf = RECV_BUF_SIZE;
    socklen_t optlen;
    struct sockaddr_in addr, client;
    socklen_t client_len = sizeof(client);
    char buf[BUFSIZE];

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    if (bind(listenfd, (struct sockaddr *)&addr,
             sizeof(addr)) == -1) {
        perror("bind");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    if (listen(listenfd, SOMAXCONN) == -1) {
        perror("listen");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    printf("Listening on %s:%d\n", SERVER_IP, SERVER_PORT);
    fflush(stdout);

    connfd = accept(listenfd, (struct sockaddr *)&client,
                    &client_len);
    if (connfd == -1) {
        perror("accept");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    if (setsockopt(connfd, SOL_SOCKET, SO_RCVBUF,
                   &rcvbuf, sizeof(rcvbuf)) == -1) {
        perror("setsockopt SO_RCVBUF");
        close(connfd);
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    optlen = sizeof(rcvbuf);
    if (getsockopt(connfd, SOL_SOCKET, SO_RCVBUF,
                   &rcvbuf, &optlen) == -1) {
        perror("getsockopt SO_RCVBUF");
        close(connfd);
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    printf("Receive buffer: %d bytes\n", rcvbuf);
    printf("Waiting for data...\n");
    fflush(stdout);

    for (;;) {
        ssize_t nr = recv(connfd, buf, sizeof(buf) - 1, 0);

        if (nr == -1) {
            if (errno == EINTR)
                continue;
            perror("recv");
            break;
        }

        if (nr == 0) {
            printf("Client disconnected\n");
            break;
        }

        buf[nr] = '\0';
        printf("Received: %s\n", buf);
        fflush(stdout);
    }

    close(connfd);
    close(listenfd);
    return 0;
}
