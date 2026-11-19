#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>

#define PORT       12346
#define MAX_CLIENTS 100
#define BUFSIZE    1024

int main(void)
{
    int listenfd;
    struct pollfd fds[MAX_CLIENTS + 1];
    struct sockaddr_in addr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

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

    /* The first entry monitors the listening socket */
    fds[0].fd = listenfd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    /* The remaining entries are initially unused */
    for (int i = 1; i <= MAX_CLIENTS; i++) {
        fds[i].fd = -1;
        fds[i].events = 0;
        fds[i].revents = 0;
    }

    printf("Server listening on port %d\n", PORT);

    for (;;) {
        int ready;
        ready = poll(fds, MAX_CLIENTS + 1, -1);
        if (ready == -1) {
            if (errno == EINTR)
                continue;
            perror("poll");
            break;
        }

        /* Check the listening socket first */
        if (fds[0].revents & POLLIN) {
            int clientfd;
            clientfd = accept(listenfd, NULL, NULL);
            if (clientfd == -1) {
                perror("accept");
            } else {
                int added = 0;
                for (int i = 1; i <= MAX_CLIENTS; i++) {
                    if (fds[i].fd == -1) {
                        fds[i].fd = clientfd;
                        fds[i].events = POLLIN;
                        fds[i].revents = 0;

                        added = 1;
                        printf("Client connected: fd=%d\n", clientfd);
                        break;
                    }
                }

                if (!added) {
                    fprintf(stderr, "Too many clients\n");
                    close(clientfd);
                }
            }
            ready--;
        }

        /* Check the connected clients */
        for (int i = 1; i <= MAX_CLIENTS && ready > 0; i++) {

            if (fds[i].fd == -1) continue;
            if (fds[i].revents == 0) continue;
            ready--;

            if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                printf("Closing client: fd=%d\n", fds[i].fd);
                close(fds[i].fd);
                fds[i].fd = -1;
                fds[i].events = 0;
                fds[i].revents = 0;
                continue;
            }

            if (fds[i].revents & POLLIN) {
                char buf[BUFSIZE + 1];
                ssize_t n;

                n = read(fds[i].fd, buf, BUFSIZE);
                if (n == -1) {
                    perror("read");
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    fds[i].events = 0;
                    fds[i].revents = 0;
                    continue;
                }

                if (n == 0) {
                    printf("Client disconnected: fd=%d\n", fds[i].fd);
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    fds[i].events = 0;
                    fds[i].revents = 0;
                    continue;
                }

                /* Echo the received data */
                if (write(fds[i].fd, buf, n) == -1) {
                    perror("write");
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    fds[i].events = 0;
                    fds[i].revents = 0;
                    continue;
                }
                buf[n] = '\0';
                printf("Received data: %s", buf);
            }
        }
    }

    for (int i = 0; i <= MAX_CLIENTS; i++) {
        if (fds[i].fd != -1)
            close(fds[i].fd);
    }
    return 0;
}