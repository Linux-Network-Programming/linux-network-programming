#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define PORT 12346
#define BUFFER_SIZE 1024

int main(void)
{
    int server_fd;
    int client_fd;
    int max_fd;
    int n;
    char buffer[BUFFER_SIZE];

    struct sockaddr_in addr;

    fd_set master_set;
    fd_set read_set;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) == -1) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    FD_ZERO(&master_set);
    FD_SET(server_fd, &master_set);
    max_fd = server_fd;

    printf("Server listening on port %d\n", PORT);
    for (;;) {

        read_set = master_set;
        n = select(max_fd + 1, &read_set, NULL, NULL, NULL);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }

        for (int fd = 0; fd <= max_fd; fd++) {
            if (!FD_ISSET(fd, &read_set))
                continue;

            if (fd == server_fd) {
                client_fd = accept(server_fd, NULL, NULL);
                if (client_fd == -1) {
                    perror("accept");
                    continue;
                }

                FD_SET(client_fd, &master_set);
                if (client_fd > max_fd)
                    max_fd = client_fd;
                printf("New client connected: fd=%d\n", client_fd);
            }
            else {
                n = read(fd, buffer, sizeof(buffer) - 1);
                if (n == -1) {
                    perror("read");
                    close(fd);
                    FD_CLR(fd, &master_set);
                    continue;
                }

                if (n == 0) {
                    printf("Client disconnected: fd=%d\n", fd);
                    close(fd);
                    FD_CLR(fd, &master_set);
                    continue;
                }

                buffer[n] = '\0';
                printf("Received from fd=%d: %s", fd, buffer);
            }
        }
    }
    close(server_fd);
    return 0;
}