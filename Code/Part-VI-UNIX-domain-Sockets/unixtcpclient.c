#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#define UNIX_SOCKET_PATH "control.socket"
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 12346

static void handle_client(int client_fd)
{
    int tcp_fd;
    struct sockaddr_in server;
    char buf[256];
    ssize_t n;

    tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_fd < 0) {
        perror("socket");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server.sin_addr) <= 0) {
        perror("inet_pton");
        close(tcp_fd);
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    if (connect(tcp_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect");
        close(tcp_fd);
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to TCP server\n");
    while ((n = read(client_fd, buf, sizeof(buf))) > 0) {
        if (write(tcp_fd, buf, n) < 0) {
            perror("write");
            break;
        }
    }
    close(tcp_fd);
    close(client_fd);
    exit(EXIT_SUCCESS);
}

int main(void)
{
    int unix_fd;
    int client_fd;
    struct sockaddr_un un;
    int size;

    signal(SIGCHLD, SIG_IGN);
    unix_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (unix_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    un.sun_family = AF_UNIX;
    strncpy(un.sun_path, UNIX_SOCKET_PATH, sizeof(un.sun_path) - 1);
    un.sun_path[sizeof(un.sun_path) - 1] = '\0';
    size = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);
    unlink(UNIX_SOCKET_PATH);

    if (bind(unix_fd, (struct sockaddr *)&un, size) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(unix_fd, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for local clients...\n");
    while (1) {
        client_fd = accept(unix_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        if (fork() == 0) {
            close(unix_fd);
            handle_client(client_fd);
        }

        close(client_fd);
    }

    close(unix_fd);
    unlink(UNIX_SOCKET_PATH);
    return 0;
}