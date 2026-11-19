#include "server.h"

#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 12346
#define WORKERS     3

static void worker(int id)
{
    int sockfd;
    struct sockaddr_in addr;
    char buf[BUFSIZE];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Worker %d (PID %ld) listening on %s:%d\n", id, (long)getpid(), SERVER_IP, SERVER_PORT);
    fflush(stdout);

    for (;;) {
        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);

        ssize_t nr = recvfrom(sockfd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&client,
                              &client_len);
        if (nr == -1) {
            if (errno == EINTR) continue;
            perror("recvfrom");
            break;
        }
        buf[nr] = '\0';
        printf("Worker %d (PID %ld) received: %s\n", id, (long)getpid(), buf);
        fflush(stdout);
    }
    close(sockfd);
    exit(EXIT_FAILURE);
}

int main(void)
{
    for (int i = 0; i < WORKERS; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0)
            worker(i + 1);
    }

    for (int i = 0; i < WORKERS; i++)
        wait(NULL);

    return 0;
}
