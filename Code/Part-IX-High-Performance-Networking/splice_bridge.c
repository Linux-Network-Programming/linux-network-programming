#include "server.h"

int main(void)
{
    int server_fd, sender_fd, receiver_fd;
    int pipefd[2];
    struct sockaddr_in addr;
    int ret = EXIT_FAILURE;

    signal(SIGPIPE, SIG_IGN);

    server_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (server_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        goto out_server;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        goto out_server;
    }

    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        goto out_server;
    }
    printf("TCP bridge listening on port %d...\n", PORT);

    do {
        sender_fd = accept4(server_fd, NULL, NULL, SOCK_CLOEXEC);
    } while (sender_fd == -1 && errno == EINTR);

    if (sender_fd == -1) {
        perror("accept4");
        goto out_server;
    }

    printf("Sender connected.\n");
    printf("Waiting for receiver...\n");

    do {
        receiver_fd = accept4(server_fd, NULL, NULL, SOCK_CLOEXEC);
    } while (receiver_fd == -1 && errno == EINTR);

    if (receiver_fd == -1) {
        perror("accept4");
        goto out_sender;
    }
    printf("Receiver connected. Forwarding data...\n");

    if (pipe(pipefd) == -1) {
        perror("pipe");
        goto out_receiver;
    }

    for (;;) {
        ssize_t n;

        /* Transfer data from the sender socket to the pipe. */
        do {
            n = splice(sender_fd, NULL, pipefd[1], NULL, BUFSIZE, 0);
        } while (n == -1 && errno == EINTR);

        if (n == 0) {
            printf("Sender disconnected.\n");
            ret = EXIT_SUCCESS;
            break;
        }

        if (n == -1) {
            perror("splice sender -> pipe");
            break;
        }

        /* Forward all bytes from the pipe to the receiver. */
        ssize_t remaining = n;
        while (remaining > 0) {
            ssize_t written;

            do {
                written = splice(pipefd[0], NULL, receiver_fd, NULL, remaining, 0);
            } while (written == -1 && errno == EINTR);

            if (written == -1) {
                perror("splice pipe -> receiver");
                goto out_pipe;
            }

            if (written == 0) {
                fprintf(stderr, "Unexpected end of pipe\n");
                goto out_pipe;
            }

            remaining -= written;
        }
    }

out_pipe:
    close(pipefd[0]);
    close(pipefd[1]);
out_receiver:
    close(receiver_fd);
out_sender:
    close(sender_fd);
out_server:
    close(server_fd);
    return ret;
}
