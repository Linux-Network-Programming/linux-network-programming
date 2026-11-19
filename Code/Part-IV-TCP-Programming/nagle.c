#include <linux/tcp.h>
#include "lpheader.h"

int main (int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "\nUsage: %s ip_server on|off", argv[0]);
        exit(EXIT_FAILURE);
    }

     if (strcmp(argv[2], "on") != 0 && strcmp(argv[2], "off") != 0) {
        fprintf(stderr, "The second argument must be on or off\n");
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

    /* Disable Nagle's algorithm. */
    if (strcmp(argv[2], "off") == 0) {
        int enable = 1;
        if(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable)) == -1) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }
    }
    
    if (connect(sockfd, (struct sockaddr *) &daddr, sizeof(struct sockaddr_in)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    char buf[] = "Hello Nagle";
    for (int i=0 ; i < 20; i++ ) {
        if (write(sockfd, buf, strlen(buf)) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }
    }
    char rbuf[BUFSZ];
    while (read(sockfd, rbuf, sizeof(rbuf)) > 0)
        ;
    
    close(sockfd);
    return 0;
}