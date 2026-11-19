#include "server.h"

static int source_pipe[2];
static size_t snapshot_len;
static pthread_mutex_t pipe_mutex = PTHREAD_MUTEX_INITIALIZER;

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

    if (pthread_mutex_lock(&pipe_mutex) != 0)
        return -1;

    /* Remove the previous snapshot */
    char discard[512];

    for (;;) {
        ssize_t n = read(source_pipe[0], discard, sizeof(discard));

        if (n > 0)
            continue;
        if (n == -1 && errno == EINTR)
            continue;
        if (n == -1 &&
            (errno == EAGAIN || errno == EWOULDBLOCK))
            break;

        pthread_mutex_unlock(&pipe_mutex);
        return -1;
    }

    /* Store the new snapshot */
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(source_pipe[1], buf + written, len - written);

        if (n > 0)
            written += (size_t)n;
        else if (n == -1 && errno == EINTR)
            continue;
        else {
            pthread_mutex_unlock(&pipe_mutex);
            return -1;
        }
    }
    snapshot_len = len;
    pthread_mutex_unlock(&pipe_mutex);
    return 0;
}

static void *snapshot_worker(void *arg)
{
    (void)arg;

    for (;;) {
        if (refresh_snapshot() == -1)
            perror("refresh_snapshot");
        sleep(1);
    }
    return NULL;
}

static void *client_worker(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    int client_pipe[2];

    if (pipe(client_pipe) == -1) {
        perror("pipe");
        close(client_fd);
        return NULL;
    }

    /* Duplicate the snapshot while protecting the source pipe */
    if (pthread_mutex_lock(&pipe_mutex) != 0) {
        close(client_pipe[0]);
        close(client_pipe[1]);
        close(client_fd);
        return NULL;
    }

    size_t len = snapshot_len;
    ssize_t n = tee(source_pipe[0], client_pipe[1], len, 0);
    pthread_mutex_unlock(&pipe_mutex);

    if (n == -1 || (size_t)n != len) {
        if (n == -1)
            perror("tee");
        else
            fprintf(stderr, "tee: incomplete snapshot\n");

        close(client_pipe[0]);
        close(client_pipe[1]);
        close(client_fd);
        return NULL;
    }

    /* Send the duplicated snapshot to the client */
    size_t remaining = len;
    while (remaining > 0) {
        ssize_t sent = splice(client_pipe[0], NULL, client_fd, NULL, remaining, 0);
        if (sent > 0)
            remaining -= (size_t)sent;
        else if (sent == -1 && errno == EINTR)
            continue;
        else {
            if (sent == -1)
                perror("splice");
            break;
        }
    }
    close(client_pipe[0]);
    close(client_pipe[1]);
    close(client_fd);
    return NULL;
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);

    if (pipe2(source_pipe, O_CLOEXEC | O_NONBLOCK) == -1) {
        perror("pipe2");
        return EXIT_FAILURE;
    }

    pthread_t worker;
    int err = pthread_create(&worker, NULL, snapshot_worker, NULL);
    if (err != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    pthread_detach(worker);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
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

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1 ||
        listen(listen_fd, BACKLOG) == -1) {
        perror("bind/listen");
        return EXIT_FAILURE;
    }
    printf("tee server listening on port %d\n", PORT);

    for (;;) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }

        int *arg = malloc(sizeof(*arg));
        if (arg == NULL) {
            perror("malloc");
            close(client_fd);
            continue;
        }
        *arg = client_fd;

        pthread_t thread;
        err = pthread_create(&thread, NULL, client_worker, arg);
        if (err != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(err));
            free(arg);
            close(client_fd);
            continue;
        }
        pthread_detach(thread);
    }
    return EXIT_SUCCESS;
}
