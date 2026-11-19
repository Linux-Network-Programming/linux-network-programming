#include "io_uring_server.h"

int main(void)
{
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
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

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(PORT)
    };

    if (bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(listenfd);
        return EXIT_FAILURE;
    }

    if (listen(listenfd, BACKLOG) < 0) {
        perror("listen");
        close(listenfd);
        return EXIT_FAILURE;
    }

    struct io_uring ring;
    int ret = io_uring_queue_init(QUEUE_DEPTH, &ring, 0);
    if (ret < 0) {
        fprintf(stderr, "io_uring_queue_init: %s\n", strerror(-ret));
        close(listenfd);
        return EXIT_FAILURE;
    }

    if (submit_accept(&ring, listenfd) < 0) {
        io_uring_queue_exit(&ring);
        close(listenfd);
        return EXIT_FAILURE;
    }

    printf("Server listening on port %d\n", PORT);
    for (;;) {
        struct io_uring_cqe *cqe;

        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) {
            if (ret == -EINTR)
                continue;
            fprintf(stderr, "io_uring_wait_cqe: %s\n", strerror(-ret));
            break;
        }

        struct request *req = io_uring_cqe_get_data(cqe);
        int res = cqe->res;

        io_uring_cqe_seen(&ring, cqe);
        if (req->type == OP_ACCEPT) {
            /* Submit another accept request independently of 
               the processing of this connection */
            if (submit_accept(&ring, listenfd) < 0) {
                free(req);
                break;
            }

            if (res < 0) {
                fprintf(stderr, "accept failed: %s\n", strerror(-res));
                free(req);
                continue;
            }

            int clientfd = res;
            char addrbuf[INET_ADDRSTRLEN];

            inet_ntop(AF_INET, &req->client_addr.sin_addr, addrbuf, sizeof(addrbuf));
            printf("Connection accepted: client fd = %d\n", clientfd);
            printf("Client address: %s:%d\n", addrbuf, ntohs(req->client_addr.sin_port));
            free(req);
            submit_recv(&ring, clientfd);

        } else if (req->type == OP_RECV) {
            if (res < 0) {
                fprintf(stderr, "recv failed on fd %d: %s\n", req->clientfd, strerror(-res));
                close(req->clientfd);
                free(req);
            } else if (res == 0) {
                printf("Client fd %d closed the connection\n", req->clientfd);
                close(req->clientfd);
                free(req);
            } else {
                req->buf[res] = '\0';
                printf("Data received from fd %d: %s\n", req->clientfd, req->buf);

                /* Reuse this request context for the next
                   receive operation on the same connection */
                struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);

                if (sqe == NULL) {
                    fprintf(stderr, "io_uring_get_sqe failed\n");
                    close(req->clientfd);
                    free(req);
                    continue;
                }

                io_uring_prep_recv(sqe, req->clientfd, req->buf, sizeof(req->buf) - 1, 0);
                io_uring_sqe_set_data(sqe, req);

                ret = io_uring_submit(&ring);
                if (ret < 0) {
                    fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
                    close(req->clientfd);
                    free(req);
                }
            }
        }
    }

    io_uring_queue_exit(&ring);
    close(listenfd);
    return EXIT_FAILURE;
}
