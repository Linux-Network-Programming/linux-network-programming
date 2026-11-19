#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENTS  64
#define MAX_CLIENTS 100
#define OUTBUF_SIZE 65536

struct client {
    int fd;
    int active;
    char outbuf[OUTBUF_SIZE];
    size_t out_len;
    struct client *next;
};

static struct client *clients;
static int epfd;
static int client_count;

static char listener_tag;

/* Remove a client from epoll and close its socket */
static void close_client(struct client *c)
{
    if (!c->active)
        return;

    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, NULL);
    close(c->fd);

    printf("Client disconnected (fd=%d)\n", c->fd);

    c->active = 0;
    c->out_len = 0;
    client_count--;
}

/* Update the events monitored for a client */
static int update_events(struct client *c)
{
    struct epoll_event ev = {0};

    ev.events = EPOLLIN | EPOLLRDHUP;

    if (c->out_len > 0)
        ev.events |= EPOLLOUT;

    ev.data.ptr = c;

    return epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
}

/* Queue data for transmission to a client */
static void queue_data(struct client *c,
                       const char *buf, size_t len)
{
    if (!c->active)
        return;

    if (len > OUTBUF_SIZE - c->out_len) {
        fprintf(stderr, "Client fd=%d: output buffer full\n", c->fd);
        close_client(c);
        return;
    }

    memcpy(c->outbuf + c->out_len, buf, len);
    c->out_len += len;

    if (update_events(c) == -1)
        close_client(c);
}

/* Forward received data to all other connected clients. */
static void broadcast(struct client *sender,
                      const char *buf, size_t len)
{
    struct client *c;

    for (c = clients; c != NULL; c = c->next) {
        if (c != sender && c->active)
            queue_data(c, buf, len);
    }
}

/* Accept incoming connections. */
static void accept_clients(int listenfd)
{
    while (1) {
        int fd = accept4(listenfd, NULL, NULL,
                         SOCK_NONBLOCK | SOCK_CLOEXEC);

        if (fd == -1) {
            if (errno == EINTR)
                continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;

            perror("accept4");
            break;
        }

        if (client_count >= MAX_CLIENTS) {
            close(fd);
            continue;
        }

        struct client *c = calloc(1, sizeof(*c));

        if (c == NULL) {
            perror("calloc");
            close(fd);
            continue;
        }

        c->fd = fd;
        c->active = 1;

        c->next = clients;
        clients = c;

        struct epoll_event ev = {0};

        ev.events = EPOLLIN | EPOLLRDHUP;
        ev.data.ptr = c;

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
            perror("epoll_ctl: ADD client");
            close(fd);
            c->active = 0;
            continue;
        }

        client_count++;

        printf("Client connected (fd=%d)\n", fd);
    }
}

/* Receive data from a client. */
static void receive_data(struct client *c)
{
    char buf[4096];

    while (1) {
        ssize_t n = recv(c->fd, buf, sizeof(buf), 0);

        if (n > 0) {
            broadcast(c, buf, (size_t)n);
            continue;
        }

        if (n == 0) {
            close_client(c);
            return;
        }

        if (errno == EINTR)
            continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;

        perror("recv");
        close_client(c);
        return;
    }
}

/* Send pending data to a client. */
static void send_data(struct client *c)
{
    while (c->out_len > 0) {
        ssize_t n = send(c->fd, c->outbuf,
                         c->out_len, MSG_NOSIGNAL);

        if (n > 0) {
            c->out_len -= (size_t)n;

            memmove(c->outbuf, c->outbuf + n,
                    c->out_len);

            continue;
        }

        if (n == -1 && errno == EINTR)
            continue;

        if (n == -1 &&
            (errno == EAGAIN || errno == EWOULDBLOCK))
            break;

        if (n == 0)
            break;

        perror("send");
        close_client(c);
        return;
    }

    if (c->active && update_events(c) == -1)
        close_client(c);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *end;
    long port = strtol(argv[1], &end, 10);

    if (*argv[1] == '\0' || *end != '\0' || port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        exit(EXIT_FAILURE);
    }

    int listenfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listenfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int enable = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1) {
        perror("setsockopt");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in saddr = {0};

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons((uint16_t)port);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
        perror("bind");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    if (listen(listenfd, SOMAXCONN) == -1) {
        perror("listen");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    epfd = epoll_create1(EPOLL_CLOEXEC);

    if (epfd == -1) {
        perror("epoll_create1");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    struct epoll_event ev = {0};

    ev.events = EPOLLIN;
    ev.data.ptr = &listener_tag;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) == -1) {
        perror("epoll_ctl: ADD listener");
        close(epfd);
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    printf("Chat server listening on port %ld\n", port);

    struct epoll_event events[MAX_EVENTS];

    while (1) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);

        if (n == -1) {
            if (errno == EINTR)
                continue;

            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++) {
            if (events[i].data.ptr == &listener_tag) {
                accept_clients(listenfd);
                continue;
            }

            struct client *c = events[i].data.ptr;

            if (!c->active)
                continue;

            if (events[i].events & EPOLLIN)
                receive_data(c);

            if (!c->active)
                continue;

            if (events[i].events & EPOLLOUT)
                send_data(c);

            if (!c->active)
                continue;

            if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
                close_client(c);
        }
    }

    close(listenfd);
    close(epfd);

    return 0;
}
