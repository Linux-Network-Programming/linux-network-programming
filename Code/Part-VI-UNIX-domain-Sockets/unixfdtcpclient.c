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

#define UNIX_SOCKET_PATH "control.socket"
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 12346

static void send_fd(int unix_fd, int tcp_fd)
{
    struct msghdr msg;
    struct cmsghdr *cmsg;
    struct iovec iov;
    char data = 'F';
    char control[CMSG_SPACE(sizeof(int))];

    memset(&msg, 0, sizeof(msg));
    memset(control, 0, sizeof(control));

    iov.iov_base = &data;
    iov.iov_len = sizeof(data);

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &tcp_fd, sizeof(int));
    
    if (sendmsg(unix_fd, &msg, 0) < 0) 
        perror("sendmsg");
}

static void handle_client(int client_fd)
{
    int tcp_fd;
    struct sockaddr_in server;

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
    send_fd(client_fd, tcp_fd);
    printf("TCP socket passed to CLI\n");

    /* The CLI now owns a descriptor referring to the same underlying TCP socket */
    close(tcp_fd);
    close(client_fd);
    exit(EXIT_SUCCESS);
}

int main(void)
{
    int unix_fd;
    int client_fd;
    int size;
    struct sockaddr_un un;

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
        close(unix_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(unix_fd, 5) < 0) {
        perror("listen");
        close(unix_fd);
        unlink(UNIX_SOCKET_PATH);
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