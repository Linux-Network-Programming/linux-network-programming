#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

int main (int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "\nUsage: %s broadcast_address", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in daddr = {0};
    int sockfd;
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
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

    socklen_t dest_len = sizeof(daddr);
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    ssize_t n;
    char buf[] = "Host request";
    n = sendto(sockfd, buf, sizeof(buf), 0, (struct sockaddr *) &daddr, dest_len);

    if (n == -1) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }

    printf("Sent %zd bytes\n", n);

    /* Read the responses */ 
    struct timeval tv = {
        .tv_sec = 1,
        .tv_usec = 0
    };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    struct sockaddr_in server = {0};
    socklen_t server_len = sizeof(server);
    char server_addr[INET_ADDRSTRLEN];

    while (1) {
        server_len = sizeof(server);
        n = recvfrom(sockfd, buf, sizeof(buf) - 1, 0,(struct sockaddr *) &server, &server_len);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            perror("recvfrom");
            exit(EXIT_FAILURE);
        }
        buf[n] = '\0';

        /* print responder address and hostname */
        if (inet_ntop(AF_INET, &server.sin_addr, server_addr, sizeof(server_addr)) == NULL) {
            perror("inet_ntop");
            exit(EXIT_FAILURE);
        }

        printf("Reply from: %s:%d: %s\n", server_addr, ntohs(server.sin_port), buf);
    }
    
    close(sockfd);
    return 0;
}