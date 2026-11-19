#include "zero_copy_server.h"

int main(void)
{
    struct io_uring ring;
    struct sockaddr_in addr;
    int listen_fd;
    int ret;

    listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
        listen(listen_fd, SOMAXCONN) < 0) {
        perror("bind/listen");
        close(listen_fd);
        return 1;
    }

    ret = io_uring_queue_init(QUEUE_DEPTH, &ring, 0);
    if (ret < 0) {
        fprintf(stderr, "io_uring setup: %s\n", strerror(-ret));
        close(listen_fd);
        return 1;
    }

    printf("Zero-copy echo server listening on port %d\n", PORT);

    if (queue_accept(&ring, listen_fd) < 0 ||
        io_uring_submit(&ring) < 0) {
        fprintf(stderr, "Failed to submit accept\n");
        io_uring_queue_exit(&ring);
        close(listen_fd);
        return 1;
    }

    for (;;) {
        struct io_uring_cqe *cqe;

        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) {
            if (ret == -EINTR)
                continue;

            fprintf(stderr, "wait: %s\n", strerror(-ret));
            break;
        }

        struct operation *op = io_uring_cqe_get_data(cqe);
        int res = cqe->res;
        unsigned flags = cqe->flags;

        if (op->type == OP_ACCEPT) {
            free(op);

            /* Keep accepting new clients. */
            if (queue_accept(&ring, listen_fd) < 0) {
                io_uring_cqe_seen(&ring, cqe);
                break;
            }

            if (res >= 0) {
                struct connection *conn = calloc(1, sizeof(*conn));

                if (!conn) {
                    close(res);
                } else {
                    conn->fd = res;
                    if (queue_recv(&ring, conn) < 0)
                        close_connection(conn);
                }
            } else {
                fprintf(stderr, "accept: %s\n", strerror(-res));
            }
        } else if (op->type == OP_RECV) {
            struct connection *conn = op->conn;
            free(op);

            if (res <= 0) {
                if (res < 0)
                    fprintf(stderr, "recv: %s\n", strerror(-res));

                close_connection(conn);
            } else {
                conn->len = (size_t)res;
                start_send(&ring, conn);
            }
        } else if (op->type == OP_SEND) {
            if (flags & IORING_CQE_F_NOTIF) {
                /*
                 * The kernel no longer needs this
                 * send buffer. It is safe to reuse.
                 */
                finish_send(&ring, op);
            } else {
                op->send_res = res;

                if (flags & IORING_CQE_F_MORE) {
                    /*
                     * A notification is expected.
                     * Keep the operation and buffer
                     * alive until that CQE arrives.
                     */
                } else {
                    /*
                     * No notification is expected.
                     * The buffer can be reused now.
                     */
                    finish_send(&ring, op);
                }
            }
        }

        io_uring_cqe_seen(&ring, cqe);

        ret = io_uring_submit(&ring);
        if (ret < 0) {
            fprintf(stderr, "submit: %s\n", strerror(-ret));
            break;
        }
    }
    io_uring_queue_exit(&ring);
    close(listen_fd);
    return 0;
}
