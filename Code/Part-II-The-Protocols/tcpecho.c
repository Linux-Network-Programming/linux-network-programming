#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 12346
#define BUFSZ 1024

int main(int argc, char *argv[])
{

    if (argc != 2) {
        fprintf(stderr, "Usage: %s ip_server", argv[0]);
        exit(EXIT_FAILURE);
    }


    int sockfd;
    struct sockaddr_in server = {0};
    char buffer[BUFSZ];
    const char *message = "Hello, TCP server!";

    /* Create the TCP socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* Specify the server address */
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if (inet_pton(AF_INET, argv[1], &server.sin_addr) != 1) {
        perror("inet_pton");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    /* Establish the TCP connection */
    if (connect(sockfd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("connect");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    /* Send the message */
    if (send(sockfd, message, strlen(message), 0) == -1) {
        perror("send");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    /* Receive the echoed message */
    ssize_t n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (n == -1) {
        perror("recv");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    buffer[n] = '\0';
    printf("Server: %s\n", buffer);
    close(sockfd);
    return 0;
}