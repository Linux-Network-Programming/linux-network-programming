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
    ssize_t n;

    while ((n = read_message(sockfd, buf, BUFSZ)) > 0) {
        if (write_all(STDOUT_FILENO, buf, n) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }
        
    }

    close(sockfd);
    return 0;
}