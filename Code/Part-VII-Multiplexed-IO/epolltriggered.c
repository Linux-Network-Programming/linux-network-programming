#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <fcntl.h>

#define PORT        12346
#define MAX_EVENTS  10
#define BUFSIZE     10

void setnonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }
}

int main(void)
{  
    struct epoll_event ev, events[MAX_EVENTS];
    int listen_sock, conn_sock, nfds, epollfd;
    struct sockaddr_in addr;

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }
    if (listen(listen_sock, SOMAXCONN) == -1) {
        perror("listen");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    for (;;) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            if (errno == EINTR)
                continue;

            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }
        
        int n;
        for (n = 0; n < nfds; ++n) {
            if (events[n].data.fd == listen_sock) {
                conn_sock = accept(listen_sock, NULL, NULL);
                if (conn_sock == -1) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                setnonblocking(conn_sock);
                ev.events = EPOLLIN;
                ev.data.fd = conn_sock;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
                    perror("epoll_ctl: conn_sock");
                    exit(EXIT_FAILURE);
                }
            } else {
                char buf[BUFSIZE + 1];
                for (;;) {
                    ssize_t nr = read(events[n].data.fd, buf, BUFSIZE);

                    if (nr > 0) {
                        buf[nr] = '\0';
                        printf("Data received: %s\n", buf);
                        continue;
                    }

                    if (nr == 0) {
                        epoll_ctl(epollfd, EPOLL_CTL_DEL, events[n].data.fd, NULL);
                        close(events[n].data.fd);
                        break;
                    }

                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;

                    if (errno == EINTR)
                        continue;

                    perror("read");
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, events[n].data.fd, NULL);
                    close(events[n].data.fd);
                    break;
                }

/*                ssize_t nr = read(events[n].data.fd, buf, BUFSIZE);

                if (nr > 0) {
                    buf[nr] = '\0';
                    printf("Data received: %s\n", buf);
                } */
            }
        }
    }
    close(epollfd);
    close(listen_sock);
    return 0;
}
