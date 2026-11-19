#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <liburing.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT        12346
#define QUEUE_DEPTH 256
#define BACKLOG     16
#define BUFSIZE     1024
#define NR_BUFFERS  128
#define BUF_GROUP   1

struct client {
    int fd;
};

static int accept_tag;
static struct io_uring_buf_ring *buf_ring;
static struct io_uring ring;
static char buffers[NR_BUFFERS][BUFSIZE];

/* Submit a multishot accept request */
static int submit_accept(int listenfd)
{
    struct io_uring_sqe *sqe;

    sqe = io_uring_get_sqe(&ring);
    if (sqe == NULL)
        return -EAGAIN;

    io_uring_prep_multishot_accept(sqe, listenfd, NULL, NULL, SOCK_CLOEXEC);
    io_uring_sqe_set_data(sqe, &accept_tag);
    int ret = io_uring_submit(&ring);

    if (ret < 0) return ret;
    if (ret == 0) return -EIO;
    return 0;
}

/* Submit a multishot receive request for a client */
static int submit_recv(struct client *client)
{
    struct io_uring_sqe *sqe;

    sqe = io_uring_get_sqe(&ring);
    if (sqe == NULL)
        return -EAGAIN;

    io_uring_prep_recv_multishot(sqe, client->fd, NULL, 0, 0);
    sqe->flags |= IOSQE_BUFFER_SELECT;
    sqe->buf_group = BUF_GROUP;
    io_uring_sqe_set_data(sqe, client);
    int ret = io_uring_submit(&ring);

    if (ret < 0) return ret;
    if (ret == 0) return -EIO;
    return 0;
}

/* Return a consumed buffer to the provided buffer ring */
static void return_buffer(unsigned bid)
{
    io_uring_buf_ring_add(buf_ring, buffers[bid], BUFSIZE, bid, io_uring_buf_ring_mask(NR_BUFFERS), 0);
    io_uring_buf_ring_advance(buf_ring, 1);
}