#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

#define PORT 12346

int main (int argc, char *argv[]) {

    struct sockaddr_in saddr = {0};
    int sockfd;
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr *) &saddr, sizeof(saddr)) == -1){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    char hostname[NI_MAXHOST];
    if (gethostname(hostname, sizeof(hostname)) == -1) {
        perror("gethostname");
        exit(EXIT_FAILURE);
    }
    printf("Host: %s\n", hostname);
    fflush(stdout);
    
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    char buf[BUFSIZ];
    ssize_t n;
    
      while (1) {
        client_len = sizeof(client);
        n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&client, &client_len);
        if (n == -1) {
            perror("recvfrom");
            exit(EXIT_FAILURE);
        }

        char client_addr[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &client.sin_addr, client_addr, sizeof(client_addr)) == NULL) {
            perror("inet_ntop");
            exit(EXIT_FAILURE);
        }

        printf("\nDiscovery request from: %s:%d\n", client_addr, ntohs(client.sin_port));
        fflush(stdout);
        if (sendto(sockfd, hostname, strlen(hostname), 0, (struct sockaddr *)&client, client_len) == -1) {
            perror("sendto");
            exit(EXIT_FAILURE);
        }
    }   

    close(sockfd);
    return 0;
}