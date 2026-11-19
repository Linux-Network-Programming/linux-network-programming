#include "lpheader.h"

int main (int argc, char *argv[]) {

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

    ssize_t n;
    char buf[BUFSZ];
    int fd = open("newfile.txt", O_RDONLY);
    if (fd == -1) {
        perror("open");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    /* Read the first block */
    n = read(fd, buf, sizeof(buf));
    if (n == -1) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    /* Send the first block with the SYN */
    ssize_t sent = sendto(sockfd, buf, n, MSG_FASTOPEN, (struct sockaddr *)&daddr, sizeof(daddr));
    if (sent == -1) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }

    /* Send the remaining data normally */
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        if (write(sockfd, buf, n) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }
    }
    close(sockfd);
    return 0;
}