#include "lpheader.h"

int main (int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "\nUsage: %s ip_server", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in daddr = {0};
    socklen_t server_len = sizeof(daddr);
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

    int enable = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_FASTOPEN_CONNECT, &enable, sizeof(enable)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    ssize_t n;
    char buf[BUFSZ];
    int fd = open("saved.txt", O_WRONLY | O_TRUNC | O_CREAT, 0644 );
    if (fd == -1) {
        perror("open");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
   
    if (connect(sockfd, (struct sockaddr *) &daddr, sizeof(struct sockaddr_in)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    
    write(sockfd, "Hello", 5);
    while ((n = read(sockfd, buf, sizeof(buf))) > 0) {
        if (write(fd, buf, n) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }
    }
    if (n == -1) {
        perror("read");
        exit(EXIT_FAILURE);
    }
    printf("\nFile saved\n");
    close(sockfd);
    return 0;
}