#include "server.h"

static int source_pipe[2];
static size_t snapshot_len;

static int refresh_snapshot(void)
{
    char buf[BUFSIZE];
    size_t capacity = sizeof(buf), len = 0;

    int pipe_size = fcntl(source_pipe[0], F_GETPIPE_SZ);
    if (pipe_size == -1)
        return -1;

    if ((size_t)pipe_size < capacity)
        capacity = (size_t)pipe_size;

    int fd = open("/proc/meminfo", O_RDONLY);
    if (fd == -1)
        return -1;

    while (len < capacity) {
        ssize_t n = read(fd, buf + len, capacity - len);

        if (n > 0)
            len += (size_t)n;
        else if (n == 0)
            break;
        else if (errno == EINTR)
            continue;
        else {
            close(fd);
            return -1;
        }
    }
    close(fd);

    /* Remove the previous snapshot. */
    char discard[512];
    while (read(source_pipe[0], discard, sizeof(discard)) > 0)
        ;

    if (errno != EAGAIN && errno != EWOULDBLOCK)
        return -1;

    /* Store the new snapshot. */
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(source_pipe[1], buf + written,
                          len - written);

        if (n > 0)
            written += (size_t)n;
        else if (n == -1 && errno == EINTR)
            continue;
        else
            return -1;
    }

    snapshot_len = len;
    return 0;
}

static int send_snapshot(int client_fd)
{
    int client_pipe[2];

    if (pipe(client_pipe) == -1)
        return -1;

    /* Duplicate the snapshot without consuming the source data. */
    ssize_t n = tee(source_pipe[0], client_pipe[1],
                    snapshot_len, 0);

    if (n == -1 || (size_t)n != snapshot_len) {
        close(client_pipe[0]);
        close(client_pipe[1]);
        return -1;
    }

    /* Transfer the duplicated data to the client. */
    size_t remaining = snapshot_len;

    while (remaining > 0) {
        ssize_t sent = splice(client_pipe[0], NULL, client_fd,
                              NULL, remaining, 0);

        if (sent > 0)
            remaining -= (size_t)sent;
        else if (sent == -1 && errno == EINTR)
            continue;
        else {
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

    if (pipe2(source_pipe, O_CLOEXEC | O_NONBLOCK) == -1) {
        perror("pipe2");
        return EXIT_FAILURE;
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
               &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1 ||
        listen(listen_fd, BACKLOG) == -1) {
        perror("bind/listen");
        return EXIT_FAILURE;
    }

    printf("tee server listening on port %d\n", PORT);

    for (;;) {
        if (refresh_snapshot() == -1) {
            perror("refresh_snapshot");
            break;
        }

        struct pollfd pfd = { .fd = listen_fd, .events = POLLIN };

        int ret = poll(&pfd, 1, 1000);
        if (ret <= 0)
            continue;

        if (!(pfd.revents & POLLIN))
            continue;

        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        printf("Client connected\n");

        if (send_snapshot(client_fd) == 0)
            printf("Snapshot sent\n");
        else
            perror("send_snapshot");

        close(client_fd);
        printf("Client disconnected\n");
    }

    close(listen_fd);
    close(source_pipe[0]);
    close(source_pipe[1]);
    return EXIT_FAILURE;
}
