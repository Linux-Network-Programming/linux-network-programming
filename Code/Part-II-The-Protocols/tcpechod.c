#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 12346
#define BUFSZ 1024

int main(void)
{
    int sockfd, clientfd;
    struct sockaddr_in server = {0};
    char buffer[BUFSZ];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, 1) == -1) {
        perror("listen");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Waiting for connection...\n");
    clientfd = accept(sockfd, NULL, NULL);
    if (clientfd == -1) {
        perror("accept");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    ssize_t n = recv(clientfd, buffer, sizeof(buffer) - 1, 0);
    if (n == -1) {
        perror("recv");
        close(clientfd);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    buffer[n] = '\0';
    printf("Client: %s\n", buffer);

    if (send(clientfd, buffer, n, 0) == -1) {
        perror("send");
        close(clientfd);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    close(clientfd);
    close(sockfd);
    return 0;
}