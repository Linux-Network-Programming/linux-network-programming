#include "server.h"

int main(int argc, char *argv[])
{
    int file_fd, server_fd, client_fd;
    struct sockaddr_in addr;
    off_t offset = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    signal(SIGPIPE, SIG_IGN);

    file_fd = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (file_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    server_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (server_fd == -1) {
        perror("socket");
        close(file_fd);
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(server_fd);
        close(file_fd);
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(server_fd);
        close(file_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        close(server_fd);
        close(file_fd);
        exit(EXIT_FAILURE);
    }

    printf("Waiting for a client on port %d...\n", PORT);
    do {
        client_fd = accept(server_fd, NULL, NULL);
    } while (client_fd == -1 && errno == EINTR);

    if (client_fd == -1) {
        perror("accept");
        close(server_fd);
        close(file_fd);
        exit(EXIT_FAILURE);
    }
    printf("Client connected. Sending file...\n");

    for (;;) {
        ssize_t n = sendfile(client_fd, file_fd, &offset, MAX_TRANSFER);

        if (n > 0) continue;
        if (n == 0) {
            printf("File transmission completed.\n");
            break;
        }

        if (errno == EINTR) continue;
        perror("sendfile");
        break;
    }

    close(client_fd);
    close(server_fd);
    close(file_fd);
    return EXIT_SUCCESS;
}
