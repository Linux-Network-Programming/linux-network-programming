#include <arpa/inet.h>
#include <errno.h>
#include <liburing.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 12346
#define QUEUE_DEPTH 256
#define BACKLOG 16
#define BUFSIZE 1024

enum op_type {
    OP_ACCEPT,
    OP_RECV
};

struct request {
    enum op_type type;
    int clientfd;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    char buf[BUFSIZE];
};

static int submit_accept(struct io_uring *ring, int listenfd)
{
    struct request *req = calloc(1, sizeof(*req));
    if (req == NULL) {
        perror("calloc");
        return -1;
    }

    req->type = OP_ACCEPT;
    req->clientfd = -1;
    req->client_addr_len = sizeof(req->client_addr);

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (sqe == NULL) {
        fprintf(stderr, "io_uring_get_sqe failed\n");
        free(req);
        return -1;
    }

    io_uring_prep_accept(sqe, listenfd, (struct sockaddr *)&req->client_addr, &req->client_addr_len, 0);
    io_uring_sqe_set_data(sqe, req);

    int ret = io_uring_submit(ring);
    if (ret < 0) {
        fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
        free(req);
        return -1;
    }

    return 0;
}

static int submit_recv(struct io_uring *ring, int clientfd)
{
    struct request *req = calloc(1, sizeof(*req));
    if (req == NULL) {
        perror("calloc");
        close(clientfd);
        return -1;
    }

    req->type = OP_RECV;
    req->clientfd = clientfd;

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (sqe == NULL) {
        fprintf(stderr, "io_uring_get_sqe failed\n");
        close(clientfd);
        free(req);
        return -1;
    }

    io_uring_prep_recv(sqe, clientfd, req->buf, sizeof(req->buf) - 1, 0);
    io_uring_sqe_set_data(sqe, req);

    int ret = io_uring_submit(ring);
    if (ret < 0) {
        fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
        close(clientfd);
        free(req);
        return -1;
    }

    return 0;
}