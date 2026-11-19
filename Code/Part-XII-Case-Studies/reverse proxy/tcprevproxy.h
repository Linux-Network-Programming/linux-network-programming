#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENTS 128
#define BUF_SIZE 65536

enum { CLIENT = 0, BACKEND = 1 };

typedef struct {
    char data[BUF_SIZE];
    size_t len;
} Buffer;

typedef struct Connection Connection;

typedef struct {
    Connection *conn;
    int side;
    int fd;
} Endpoint;

struct Connection {
    Endpoint ep[2];
    Buffer buf[2];
    int read_eof[2];
    int write_shutdown[2];
    int connecting;
    int dead;
    Connection *next;
};

static int epfd;
static Connection *connections;

/* Set a socket to nonblocking mode */
static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* Remove a connection from epoll and close its sockets */
static void destroy_connection(Connection *c)
{
    if (c->dead) return;
    c->dead = 1;

    for (int i = 0; i < 2; i++) {
        if (c->ep[i].fd >= 0) {
            epoll_ctl(epfd, EPOLL_CTL_DEL, c->ep[i].fd, NULL);
            close(c->ep[i].fd);
            c->ep[i].fd = -1;
        }
    }
}

/* Free connections only after processing the current epoll batch */
static void collect_connections(void)
{
    Connection **p = &connections;
    while (*p) {
        Connection *c = *p;

        if (c->dead) {
            *p = c->next;
            free(c);
        } else {
            p = &c->next;
        }
    }
}

/* Update the events monitored for an endpoint */
static int update_endpoint(Connection *c, int side)
{
    Endpoint *e = &c->ep[side];
    Endpoint *peer = &c->ep[1 - side];

    if (c->dead || e->fd < 0) return -1;
    uint32_t events = EPOLLRDHUP | EPOLLERR;

    if (c->connecting && side == BACKEND)
        events |= EPOLLOUT;
    else {
        if (!c->read_eof[side] && c->buf[side].len < BUF_SIZE)
            events |= EPOLLIN;
        if (!c->connecting && c->buf[1 - side].len > 0)
            events |= EPOLLOUT;
    }

    struct epoll_event ev = {
        .events = events,
        .data.ptr = e
    };
    return epoll_ctl(epfd, EPOLL_CTL_MOD, e->fd, &ev);
}

/* Read data from an endpoint into its forwarding buffer */
static void receive_data(Connection *c, int side)
{
    Buffer *b = &c->buf[side];

    while (b->len < BUF_SIZE) {
        ssize_t n = recv(c->ep[side].fd, b->data + b->len, BUF_SIZE - b->len, 0);

        if (n > 0) {
            b->len += n;
            continue;
        }

        if (n == 0) {
            c->read_eof[side] = 1;
            break;
        }

        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        destroy_connection(c);
        return;
    }
}

/* Write buffered data to the opposite endpoint */
static void send_data(Connection *c, int side)
{
    Buffer *b = &c->buf[1 - side];

    while (b->len > 0) { 
        ssize_t n = send(c->ep[side].fd, b->data, b->len, MSG_NOSIGNAL);

        if (n > 0) {
            b->len -= n;
            if (b->len > 0) memmove(b->data, b->data + n, b->len);
            continue;
        }

        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        destroy_connection(c);
        return;
    }
}

/* Propagate TCP half-closes after pending data has been forwarded */
static void propagate_shutdown(Connection *c)
{
    for (int i = 0; i < 2; i++) {
        int peer = 1 - i;

        if (c->read_eof[i] && c->buf[i].len == 0 && !c->write_shutdown[peer] &&
            c->ep[peer].fd >= 0 && !c->connecting) {

            if (shutdown(c->ep[peer].fd, SHUT_WR) == -1 && errno != ENOTCONN) {
                destroy_connection(c);
                return;
            }
            c->write_shutdown[peer] = 1;
        }
    }
}

/* Create a backend connection for an accepted client */
static void create_connection(int client_fd, const struct sockaddr_in *backend)
{
    Connection *c = calloc(1, sizeof(*c));
    if (!c) {
        close(client_fd);
        return;
    }

    c->ep[CLIENT] = (Endpoint) {
        .conn = c, .side = CLIENT, .fd = client_fd
    };

    c->ep[BACKEND] = (Endpoint) {
        .conn = c, .side = BACKEND, .fd = -1
    };

    if (set_nonblocking(client_fd) == -1) goto error;

    int backend_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (backend_fd == -1) goto error;

    c->ep[BACKEND].fd = backend_fd;
    if (set_nonblocking(backend_fd) == -1) goto error;

    int ret = connect(backend_fd, (const struct sockaddr *)backend, sizeof(*backend));
    if (ret == -1) {
        if (errno != EINPROGRESS) goto error;
        c->connecting = 1;
    }

    struct epoll_event ev = {
        .events = EPOLLIN | EPOLLRDHUP | EPOLLERR,
        .data.ptr = &c->ep[CLIENT]
    };

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
        goto error;

    ev.events = EPOLLRDHUP | EPOLLERR;

    if (c->connecting) ev.events |= EPOLLOUT;
    else ev.events |= EPOLLIN;

    ev.data.ptr = &c->ep[BACKEND];

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, backend_fd, &ev) == -1) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
        goto error;
    }

    c->next = connections;
    connections = c;
    printf("New client connection established\n");
    return;

error:
    if (c->ep[CLIENT].fd >= 0) close(c->ep[CLIENT].fd);
    if (c->ep[BACKEND].fd >= 0) close(c->ep[BACKEND].fd);
    free(c);
}

/* Process an endpoint event */
static void handle_endpoint(Endpoint *e, uint32_t events)
{
    Connection *c = e->conn;
    int side = e->side;

    if (c->dead) return;
    
    /* Handle the socket error */
    if (events & EPOLLERR) {
        int err = 0;
        socklen_t len = sizeof(err);
        if (getsockopt(e->fd, SOL_SOCKET, SO_ERROR, &err, &len) == -1 || err != 0) {
            perror("getsockopt");  // prints the error of getsockopt
            destroy_connection(c);
            return;
        }
        if (err != 0) {
            fprintf(stderr, "Socket error: %s\n", strerror(err)); // prints the socket error
            destroy_connection(c);
            return;
        }
    }

    /* Complete an asynchronous backend connection. */
    if (side == BACKEND && c->connecting) {
        if (!(events & EPOLLOUT)) return;

        int err = 0;
        socklen_t len = sizeof(err);
        if (getsockopt(e->fd, SOL_SOCKET, SO_ERROR, &err, &len) == -1 || err != 0) {
            destroy_connection(c);
            return;
        }
        c->connecting = 0;
        printf("Backend connection established\n");
    }

    if (c->connecting && side == BACKEND) return;

    if (events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP))
        receive_data(c, side);

    if (c->dead) return;
    if (events & (EPOLLOUT | EPOLLHUP)) {
        if (!c->connecting)
            send_data(c, side);
    }

    if (c->dead) return;
    propagate_shutdown(c);

    if (c->dead) return;

    /* Close when both directions have completed. */
    if (c->read_eof[CLIENT] && c->read_eof[BACKEND] && c->buf[CLIENT].len == 0 &&
        c->buf[BACKEND].len == 0) {
        destroy_connection(c);
        return;
    }
    update_endpoint(c, CLIENT);
    update_endpoint(c, BACKEND);
}
