#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <liburing.h>

#define PORT 12346
#define QUEUE_DEPTH 1

int main(void)
{
    struct io_uring ring;
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    int sockfd, clientfd;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    /* Create the listening socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    int enabled = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, SOMAXCONN) == -1) {
        perror("listen");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    /* Initialize io_uring */
    if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) {
        perror("io_uring_queue_init");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    while (1) {
         /* Get an SQE */
        sqe = io_uring_get_sqe(&ring);
        if (sqe == NULL) {
            fprintf(stderr, "io_uring_get_sqe failed\n");
            break;
        }

        client_len = sizeof(client_addr);

        /* Prepare an asynchronous accept request */
        io_uring_prep_accept(sqe, sockfd, (struct sockaddr *)&client_addr, &client_len, 0);

        /* Submit the request */
        if (io_uring_submit(&ring) < 0) {
            perror("io_uring_submit");
            break;
        }

        /* Wait for completion */
        if (io_uring_wait_cqe(&ring, &cqe) < 0) {
            perror("io_uring_wait_cqe");
            break;
        }


        /* Process the result */
        if (cqe->res <= 0) {
            fprintf(stderr, "accept failed: %s\n", strerror(-cqe->res));
        } else {
            clientfd = cqe->res;
            printf("Connection accepted: client fd = %d\n", clientfd);
            printf("Client address: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
           // close(clientfd);
        }

        /* Mark the CQE as seen */
        io_uring_cqe_seen(&ring, cqe);

        /* Receive the data */
        sqe = io_uring_get_sqe(&ring);
        if (sqe == NULL) {
            fprintf(stderr, "io_uring_get_sqe failed\n");
            close(clientfd);
            break;
        }

        char buf[BUFSIZ];
        io_uring_prep_recv(sqe, clientfd, buf, sizeof(buf) - 1, 0);

        if (io_uring_submit(&ring) < 0) {
            perror("io_uring_submit");
            close(clientfd);
            break;
        }

        if (io_uring_wait_cqe(&ring, &cqe) < 0) {
            perror("io_uring_wait_cqe");
            close(clientfd);
            break;
        }

        if (cqe->res < 0) 
            fprintf(stderr, "recv failed: %s\n", strerror(-cqe->res));
        else if (cqe->res == 0) 
            printf("Client closed the connection\n");
        else {
            buf[cqe->res] = '\0';
            printf("Data received: %s\n", buf);
        }

        io_uring_cqe_seen(&ring, cqe);
        close(clientfd);

    }

    io_uring_queue_exit(&ring);
    close(sockfd);
    return 0;
}