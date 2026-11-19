#include "io_uring_multishot_server.h"
int main(void)
{
    int listenfd;
    int ret;
    int ring_initialized = 0;
    struct sockaddr_in addr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(listenfd);
        return EXIT_FAILURE;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listenfd);
        return EXIT_FAILURE;
    }

    if (listen(listenfd, BACKLOG) < 0) {
        perror("listen");
        close(listenfd);
        return EXIT_FAILURE;
    }

    ret = io_uring_queue_init(QUEUE_DEPTH, &ring, 0);
    if (ret < 0) {
        fprintf(stderr, "io_uring_queue_init: %s\n", strerror(-ret));
        close(listenfd);
        return EXIT_FAILURE;
    }

    ring_initialized = 1;

    /* Allocate and register the provided buffer ring */
    buf_ring = io_uring_setup_buf_ring(&ring, NR_BUFFERS, BUF_GROUP, 0, &ret);
    if (buf_ring == NULL) {
        fprintf(stderr, "io_uring_setup_buf_ring: %s\n", strerror(-ret));
        goto out;
    }

    io_uring_buf_ring_init(buf_ring);
    for (unsigned i = 0; i < NR_BUFFERS; i++) 
        io_uring_buf_ring_add(buf_ring, buffers[i], BUFSIZE, i, io_uring_buf_ring_mask(NR_BUFFERS), i);
    
    io_uring_buf_ring_advance(buf_ring, NR_BUFFERS);

    /* Submit the initial multishot accept request */
    if (submit_accept(listenfd) < 0) {
        fprintf(stderr, "Cannot submit accept request\n");
        goto out;
    }
    printf("Multishot server listening on port %d\n", PORT);

    for (;;) {
        struct io_uring_cqe *cqe;
        void *data;
        unsigned flags;
        int res;

        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) {
            fprintf(stderr, "io_uring_wait_cqe: %s\n", strerror(-ret));
            break;
        }

        /* Save completion information before marking the CQE seen */
        data = io_uring_cqe_get_data(cqe);
        flags = cqe->flags;
        res = cqe->res;
        io_uring_cqe_seen(&ring, cqe);

        /* Handle multishot accept completions */
        if (data == &accept_tag) {
            if (res < 0) {
                fprintf(stderr, "accept: %s\n", strerror(-res));
                /* This example stops on an accept error */
                break;
            }

            int clientfd = res;
            struct client *client = malloc(sizeof(*client));

            if (client == NULL) {
                perror("malloc");
                close(clientfd);
            } else {
                client->fd = clientfd;
                printf("Accepted client fd=%d\n", clientfd);
                if (submit_recv(client) < 0) {
                    fprintf(stderr, "Cannot submit receive for fd=%d\n", clientfd);
                    close(clientfd);
                    free(client);
                }
            }

            /* If IORING_CQE_F_MORE is absent, the multishot accept request has terminated */
            if (!(flags & IORING_CQE_F_MORE)) {
                if (submit_accept(listenfd) < 0) {
                    fprintf(stderr, "Cannot resubmit accept request\n");
                    break;
                }
            }

            continue;
        }

        /* Handle multishot receive completions */
        struct client *client = data;
        int more = flags & IORING_CQE_F_MORE;

        if (res > 0) {
            if (!(flags & IORING_CQE_F_BUFFER)) {
                fprintf(stderr, "Receive completion has no buffer\n");
                if (!more) {
                    close(client->fd);
                    free(client);
                }
                continue;
            }

            unsigned bid = flags >> IORING_CQE_BUFFER_SHIFT;
            printf("Received %d bytes from fd=%d: ", res, client->fd);
            fwrite(buffers[bid], 1, res, stdout);
            putchar('\n');

            /* Return the consumed buffer to the buffer ring */
            return_buffer(bid);

            /* If the request has terminated, submit another 
               multishot receive request for this client */
            if (!more) {
                if (submit_recv(client) < 0) {
                    fprintf(stderr, "Cannot resubmit receive for fd=%d\n", client->fd);
                    close(client->fd);
                    free(client);
                }
            }

            continue;
        }

        if (res == 0) {
            printf("Client fd=%d disconnected\n", client->fd);
        } else {
            fprintf(stderr, "recv fd=%d: %s\n", client->fd, strerror(-res));
        }

        /* A final completion means the receive request is no longer active, 
           so its client context can be released */
        if (!more) {
            close(client->fd);
            free(client);
        }
    }

out:
    /*
     * This is a demonstration server. A production server should
     * also cancel outstanding requests and track active clients
     * for orderly shutdown.
     */
    if (buf_ring != NULL)
        io_uring_free_buf_ring(&ring, buf_ring, NR_BUFFERS, BUF_GROUP);

    if (ring_initialized)
        io_uring_queue_exit(&ring);

    close(listenfd);
    return EXIT_FAILURE;
}
