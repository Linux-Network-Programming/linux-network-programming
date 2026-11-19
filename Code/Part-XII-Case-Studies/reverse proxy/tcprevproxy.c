#include "tcprevproxy.h"

int main(int argc, char *argv[])
{
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <listen_ip> <listen_port> "
                "<backend_ip> <backend_port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct sockaddr_in listen_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(atoi(argv[2]))
    };
    struct sockaddr_in backend_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(atoi(argv[4]))
    };

    if (inet_pton(AF_INET, argv[1], &listen_addr.sin_addr) != 1 ||
        inet_pton(AF_INET, argv[3], &backend_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IP address\n");
        return EXIT_FAILURE;
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (set_nonblocking(listen_fd) == -1 || bind(listen_fd, 
       (struct sockaddr *)&listen_addr, sizeof(listen_addr)) == -1 ||
       listen(listen_fd, SOMAXCONN) == -1) {
        perror("listen setup");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) {
        perror("epoll_create1");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    struct epoll_event ev = {
        .events = EPOLLIN,
        .data.ptr = NULL
    };

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        perror("epoll_ctl");
        close(epfd);
        close(listen_fd);
        return EXIT_FAILURE;
    }

    printf("TCP proxy listening on %s:%s\n", argv[1], argv[2]);

    struct epoll_event events[MAX_EVENTS];

    while (1) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (n == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++) {
            Endpoint *e = events[i].data.ptr;

            /* The listening socket has a NULL pointer. */
            if (!e) {
                while (1) {
                    int client_fd = accept(listen_fd, NULL, NULL);
                    if (client_fd == -1) { 
                        if (errno == EINTR) continue;
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        perror("accept");
                        break;
                    }
                    create_connection(client_fd, &backend_addr);
                }
            } else {
                handle_endpoint(e, events[i].events);
            }
        }
        collect_connections();
    }
    close(listen_fd);
    close(epfd);
    return EXIT_SUCCESS;
}
