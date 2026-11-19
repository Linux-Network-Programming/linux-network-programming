#define _GNU_SOURCE
#include <liburing.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define PORT 12346
#define QUEUE_DEPTH 256
#define BUFSIZE 4096

enum op_type { OP_ACCEPT, OP_RECV, OP_SEND };

struct connection {
    int fd;
    char buf[BUFSIZE];
    size_t len;
    size_t offset;
};

struct operation {
    enum op_type type;
    struct connection *conn;
    int send_res;
};

/* Obtain an SQE, submitting pending entries if necessary. */
static struct io_uring_sqe *get_sqe(struct io_uring *ring)
{
    struct io_uring_sqe *sqe;
    while ((sqe = io_uring_get_sqe(ring)) == NULL) {
        int ret = io_uring_submit(ring);
        if (ret < 0)
            return NULL;
    }
    return sqe;
}

static int queue_accept(struct io_uring *ring, int fd)
{
    struct operation *op = calloc(1, sizeof(*op));
    if (!op) return -1;

    op->type = OP_ACCEPT;
    struct io_uring_sqe *sqe = get_sqe(ring);
    if (!sqe) {
        free(op);
        return -1;
    }

    io_uring_prep_accept(sqe, fd, NULL, NULL, SOCK_CLOEXEC);
    io_uring_sqe_set_data(sqe, op);
    return 0;
}

static int queue_recv(struct io_uring *ring, struct connection *conn)
{
    struct operation *op = calloc(1, sizeof(*op));
    if (!op) return -1;

    op->type = OP_RECV;
    op->conn = conn;
    struct io_uring_sqe *sqe = get_sqe(ring);
    if (!sqe) {
        free(op);
        return -1;
    }

    io_uring_prep_recv(sqe, conn->fd, conn->buf, sizeof(conn->buf), 0);
    io_uring_sqe_set_data(sqe, op);
    return 0;
}

static int queue_send(struct io_uring *ring,  struct operation *op)
{
    struct io_uring_sqe *sqe = get_sqe(ring);
    if (!sqe) return -1;

    io_uring_prep_send_zc(sqe, op->conn->fd, op->conn->buf + op->conn->offset,
        op->conn->len - op->conn->offset, MSG_NOSIGNAL, 0);
    io_uring_sqe_set_data(sqe, op);
    return 0;
}

static void close_connection(struct connection *conn)
{
    if (conn) {
        close(conn->fd);
        free(conn);
    }
}

static void start_send(struct io_uring *ring, struct connection *conn)
{
    struct operation *op = calloc(1, sizeof(*op));
    if (!op) {
        close_connection(conn);
        return;
    }

    op->type = OP_SEND;
    op->conn = conn;
    conn->offset = 0;
    if (queue_send(ring, op) < 0) {
        free(op);
        close_connection(conn);
    }
}

/*
 * Called only when the send buffer is safe to reuse:
 * either no notification was required, or the
 * notification CQE has arrived.
 */
static void finish_send(struct io_uring *ring, struct operation *op)
{
    struct connection *conn = op->conn;
    int res = op->send_res;
    if (res <= 0) {
        if (res < 0)
            fprintf(stderr, "send: %s\n", strerror(-res));
        free(op);
        close_connection(conn);
        return;
    }

    conn->offset += (size_t)res;
    /* Handle a partial send. */
    if (conn->offset < conn->len) {
        op->send_res = 0;

        if (queue_send(ring, op) < 0) {
            free(op);
            close_connection(conn);
        }
        return;
    }
    free(op);
    /* The complete message has been echoed. */
    if (queue_recv(ring, conn) < 0)
        close_connection(conn);
}