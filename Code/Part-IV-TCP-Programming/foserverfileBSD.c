
#include "lpheader.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "\nUsage: %s port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in saddr = {0};

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int enable = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_FASTOPEN,
                   &enable, sizeof(enable)) == -1) {
        perror("setsockopt");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(atoi(argv[1]));
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);


    if (bind(sockfd, (struct sockaddr *)&saddr,
             sizeof(saddr)) == -1) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, 10) == -1) {
        perror("listen");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    int fd = open("newfile.txt", O_RDONLY);
    if (fd == -1) {
        perror("open");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in client;
    char buf[BUFSZ];

    while (1) {
        socklen_t client_len = sizeof(client);

        int sockacc = accept(sockfd, (struct sockaddr *)&client,
                             &client_len);
        if (sockacc == -1) {
            perror("accept");
            continue;
        }

        char client_addr[INET_ADDRSTRLEN];

        if (inet_ntop(AF_INET, &client.sin_addr, client_addr,
                      sizeof(client_addr)) == NULL) {
            perror("inet_ntop");
            close(sockacc);
            continue;
        }

        printf("\nNew connection from: %s:%d\n",
               client_addr, ntohs(client.sin_port));

        /* Receive the client's initial message. */
        ssize_t n = recv(sockacc, buf, 5, MSG_WAITALL);
        if (n != 5) {
            fprintf(stderr, "Failed to receive client message\n");
            close(sockacc);
            continue;
        }

        /* Rewind the file for each client. */
        if (lseek(fd, 0, SEEK_SET) == -1) {
            perror("lseek");
            close(sockacc);
            continue;
        }

        /* Send the file. */
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            ssize_t sent = 0;

            while (sent < n) {
                ssize_t ret = write(sockacc, buf + sent, n - sent);

                if (ret == -1) {
                    if (errno == EINTR)
                        continue;

                    perror("write");
                    break;
                }

                sent += ret;
            }

            if (sent < n)
                break;
        }

        if (n == -1)
            perror("read");
        else
            printf("\nFile sent\n");

        close(sockacc);
    }

    close(fd);
    close(sockfd);

    return 0;
}
