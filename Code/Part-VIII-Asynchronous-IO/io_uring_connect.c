#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <liburing.h>

#define SERVER_ADDR "127.0.0.1"
#define PORT 12346
#define QUEUE_DEPTH 1

int main(void)
{
    struct io_uring ring;
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    int sockfd;
    struct sockaddr_in server_addr;

    /* Create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) {
        perror("io_uring_queue_init");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    sqe = io_uring_get_sqe(&ring);
    if (sqe == NULL) {
        fprintf(stderr, "io_uring_get_sqe failed\n");
        io_uring_queue_exit(&ring);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    io_uring_prep_connect(sqe, sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (io_uring_submit(&ring) < 0) {
        perror("io_uring_submit");
        io_uring_queue_exit(&ring);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (io_uring_wait_cqe(&ring, &cqe) < 0) {
        perror("io_uring_wait_cqe");
        io_uring_queue_exit(&ring);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (cqe->res < 0) {
        fprintf(stderr, "connect failed: %s\n", strerror(-cqe->res));
        io_uring_cqe_seen(&ring, cqe);
        io_uring_queue_exit(&ring);
        close(sockfd);
        return EXIT_FAILURE;
    } 
    
    printf("Connection established\n");
    io_uring_cqe_seen(&ring, cqe);

    sqe = io_uring_get_sqe(&ring);
    if (sqe == NULL) {
        fprintf(stderr, "io_uring_get_sqe failed\n");
        io_uring_queue_exit(&ring);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    char buf[] = "Hello from client";
    io_uring_prep_send(sqe, sockfd, buf, strlen(buf), 0);

    if (io_uring_submit(&ring) < 0) {
        perror("io_uring_submit");
        io_uring_queue_exit(&ring);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (io_uring_wait_cqe(&ring, &cqe) < 0) {
        perror("io_uring_wait_cqe");
        io_uring_queue_exit(&ring);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (cqe->res < 0) 
        fprintf(stderr, "send failed: %s\n", strerror(-cqe->res));
    else if ((size_t)cqe->res != strlen(buf)) 
        fprintf(stderr, "partial send: %d of %zu bytes\n", cqe->res, strlen(buf));
    else
        printf("Data sent\n");
    
    io_uring_cqe_seen(&ring, cqe);
    io_uring_queue_exit(&ring);
    close(sockfd);
    return 0;
}