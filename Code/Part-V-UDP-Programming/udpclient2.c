#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFSZ 1024

int main (int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "\nUsage: %s ip_server filename", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in daddr = {0};
    int sockfd;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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

    ssize_t n, w;
    char buf[BUFSZ];

    int fd = open(argv[2], O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }   

    while ((n = read(fd, buf, BUFSZ)) > 0) {
        w = send(sockfd, buf, n, 0);
        if (w == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }
        printf("read: %zd, send: %zd\n", n, w);

    }

    if (n == -1) {
        perror("read");
        exit(EXIT_FAILURE);
    }   
    sleep (10);
    close(fd);
    close(sockfd);
    return 0;
}