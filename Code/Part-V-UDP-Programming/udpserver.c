#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFSZ 1024

int main (int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "\nUsage: %s port", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in saddr = {0};
    
    int sockfd;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(atoi(argv[1]));
    if (inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if (bind(sockfd, (struct sockaddr *) &saddr, sizeof(saddr)) == -1){
        perror("bind");
        exit(EXIT_FAILURE);
    }
    
    ssize_t n;
    char buf[BUFSZ];
    while (1) {
      //  n = recvfrom(sockfd, buf, BUFSZ, 0, NULL, NULL);
        n = recv(sockfd, buf, BUFSZ, 0);
        if (n == -1) {
            perror("recvfrom");
            exit(EXIT_FAILURE);
        }
        printf("Received: %zd bytes\n", n);
        fflush(stdout);
    }   

    close(sockfd);
    return 0;
}