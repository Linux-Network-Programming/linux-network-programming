#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>

#define PORT        12346
#define MAX_EVENTS  100
#define BUFSIZE     1024

int main(void)
{
    int listenfd;
    int epfd;
    int clientfd;
    struct sockaddr_in addr;
    struct epoll_event event;
    struct epoll_event events[MAX_EVENTS];

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

    if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listenfd);
        exit(EXIT_FAILURE);
    }
    if (listen(listenfd, SOMAXCONN) == -1) {
        perror("listen");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    /* Create the epoll instance */
    epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("epoll_create1");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    /* Register the listening socket in the interest list */
    event.events = EPOLLIN;
    event.data.fd = listenfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &event) == -1) {
        perror("epoll_ctl");
        close(epfd);
        close(listenfd);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", PORT);

    for (;;) {
        int n;
        n = epoll_wait(epfd, events, MAX_EVENTS, -1);

        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("epoll_wait");
            break;
        }

        /* Process the returned events */
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            if (fd == listenfd) {
                clientfd = accept(listenfd, NULL, NULL);
                if (clientfd == -1) {
                    perror("accept");
                    continue;
                }
                printf("New client connected: fd=%d\n", clientfd);

                /* Register the new client socket in the interest list */
                event.events = EPOLLIN;
                event.data.fd = clientfd;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &event) == -1) {
                    perror("epoll_ctl");
                    close(clientfd);
                }
                continue;
            }

            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                printf("Client disconnected: fd=%d\n", fd);
                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
                continue;
            }

            /* A client socket is ready for reading */
            if (events[i].events & EPOLLIN) {
                char buf[BUFSIZE + 1];
                ssize_t nr = read(fd, buf, BUFSIZE);
                if (nr == -1) {
                    perror("read");
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    continue;
                }

                /* A return value of zero indicates 
                 * that the client closed the connection */
                if (nr == 0) {
                    printf("Client disconnected: fd=%d\n", fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    continue;
                }
                buf[nr] = '\0';
                printf("Received from fd=%d: %s", fd, buf);

                if (write(fd, buf, nr) == -1) {
                    perror("write");
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                }
            }
        }
    }
    close(epfd);
    close(listenfd);
    return 0;
}