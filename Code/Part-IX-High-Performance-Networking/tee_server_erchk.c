#include "server.h"

static int source_pipe[2];
static size_t snapshot_len;

static int refresh_snapshot(void)
{
    char buf[BUFSIZE];
    size_t capacity = sizeof(buf), len = 0;

    int pipe_size = fcntl(source_pipe[0], F_GETPIPE_SZ);
    if (pipe_size == -1) return -1;
    if ((size_t)pipe_size < capacity)
        capacity = (size_t)pipe_size;

    int fd = open("/proc/meminfo", O_RDONLY | O_CLOEXEC);
    if (fd == -1) return -1;

    while (len < capacity) {
        ssize_t n = read(fd, buf + len, capacity - len);

        if (n > 0) len += (size_t)n;
        else if (n == 0) break;
        else if (errno == EINTR) continue;
        else {
            close(fd);
            return -1;
        }
    }
    close(fd);

    /* Remove the previous snapshot */
    char discard[512];
    for (;;) {
        ssize_t n = read(source_pipe[0], discard, sizeof(discard));

        if (n > 0) continue;
        if (n == -1 && errno == EINTR) continue;
        if (n == -1 && errno == EAGAIN) break;
        if (n == 0) break;
        return -1;
    }

    /* Store the new snapshot in the source pipe. */
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(source_pipe[1], buf + written, len - written);
        if (n > 0) written += (size_t)n;
        else if (n == -1 && errno == EINTR) continue;
        else return -1;
    }
    snapshot_len = len;
    return 0;
}

static int send_snapshot(int client_fd)
{
    int client_pipe[2];
    if (pipe2(client_pipe, O_CLOEXEC) == -1) {
        perror("pipe2");
        return -1;
    }

    /* Duplicate the current snapshot into the client's pipe */
    ssize_t n = tee(source_pipe[0], client_pipe[1], snapshot_len, 0);
    if (n == -1) {
        perror("tee");
        close(client_pipe[0]);
        close(client_pipe[1]);
        return -1;
    }

    if ((size_t)n != snapshot_len) {
        fprintf(stderr, "tee: incomplete snapshot\n");
        close(client_pipe[0]);
        close(client_pipe[1]);
        return -1;
    }

    /* Transfer the duplicated data to the client socket */
    size_t remaining = (size_t)n;
    while (remaining > 0) {
        ssize_t sent = splice(client_pipe[0], NULL, client_fd, NULL, remaining, 0);
        if (sent > 0) remaining -= (size_t)sent; 
        else if (sent == -1 && errno == EINTR)  continue;
        else {
            if (sent == -1) perror("splice");
            else fprintf(stderr, "splice: unexpected EOF\n");
            
            close(client_pipe[0]);
            close(client_pipe[1]);
            return -1;
        }
    }
    close(client_pipe[0]);
    close(client_pipe[1]);
    return 0;
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);

    /* The source pipe must be nonblocking so it can be drained */
    if (pipe2(source_pipe, O_CLOEXEC | O_NONBLOCK) == -1) {
        perror("pipe2");
        return EXIT_FAILURE;
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listen_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return EXIT_FAILURE;
    }

    if (listen(listen_fd, BACKLOG) == -1) {
        perror("listen");
        return EXIT_FAILURE;
    }
    printf("tee server listening on port %d\n", PORT);

    for (;;) {
        /* Refresh the source pipe with the latest system information. */
        if (refresh_snapshot() == -1) {
            perror("refresh_snapshot");
            break;
        }
        printf("Updated system information (%zu bytes)\n", snapshot_len);

        struct pollfd pfd = { .fd = listen_fd, .events = POLLIN };
        int ret;
        ret = poll(&pfd, 1, 1000);
        if (ret == -1) {
            perror("poll");
            continue;
        }

        /* No connection arrived; refresh the snapshot again. */
        if (ret == 0) continue;
        if (!(pfd.revents & POLLIN)) continue;

        int client_fd;
        client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd == -1) {
            perror("accept4");
            continue;
        }
        printf("Client connected\n");

        if (send_snapshot(client_fd) == 0) 
            printf("Snapshot sent\n");

        close(client_fd);
        printf("Client disconnected\n");
    }

    close(listen_fd);
    close(source_pipe[0]);
    close(source_pipe[1]);
    return EXIT_FAILURE;
}
