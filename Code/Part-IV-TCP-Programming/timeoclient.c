#include <linux/tcp.h>
#include "lpheader.h"

#define BUFSZ 1024

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

    int timeo = 5000;
    int ro = setsockopt(sockfd, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeo, sizeof(timeo)); 
    if (ro == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    if (connect(sockfd, (struct sockaddr *) &daddr, sizeof(struct sockaddr_in)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    int fd = open("newfile.txt", O_RDONLY);
    if (fd == -1) {
        perror("open");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    ssize_t n;
    char buf[BUFSZ];

    while (1) {
        lseek(fd, 0, SEEK_SET);

        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            if (write_all(sockfd, buf, n) == -1) {
                perror("write");
                exit(EXIT_FAILURE);
            }
        }
    }

    close(sockfd);
    return 0;
}