#include "lpheader.h"

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "\nUsage: %s ip_server", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in daddr = {0};
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    daddr.sin_family = AF_INET;
    daddr.sin_port = htons(12346);
    if (inet_pton(AF_INET, argv[1], &daddr.sin_addr) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    
    if (connect(sockfd, (struct sockaddr *) &daddr, sizeof(struct sockaddr_in)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    char buf[BUFSZ];
    uint32_t length;

    while (1) {

        ssize_t n = read_all(sockfd, &length, sizeof(length));

        if (n == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        if (n == 0)
            break;

        if (n != sizeof(length)) {
            fprintf(stderr, "Incomplete message length\n");
            exit(EXIT_FAILURE);
        }

        length = ntohl(length);
        if (length > BUFSZ) {
            fprintf(stderr, "Message too large\n");
            exit(EXIT_FAILURE);
        }

        n = read_all(sockfd, buf, length);
        if (n == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        if (n != length) {
            fprintf(stderr, "Incomplete message\n");
            exit(EXIT_FAILURE);
        }

        if (write_all(STDOUT_FILENO, buf, length) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }
    }

    close(sockfd);
    return 0;
}