#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>

#define SOCKET_PATH "control.socket"

static int receive_fd(int unix_fd)
{
    struct msghdr msg;
    struct cmsghdr *cmsg;
    struct iovec iov;
    char data;
    char control[CMSG_SPACE(sizeof(int))];
    int tcp_fd = -1;

    memset(&msg, 0, sizeof(msg));
    memset(control, 0, sizeof(control));

    iov.iov_base = &data;
    iov.iov_len = sizeof(data);

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    if (recvmsg(unix_fd, &msg, 0) < 0) {
        perror("recvmsg");
        return -1;
    }

    cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == NULL) {
        fprintf(stderr, "No control message received\n");
        return -1;
    }

    if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
        fprintf(stderr, "Unexpected control message\n");
        return -1;
    }

    memcpy(&tcp_fd, CMSG_DATA(cmsg), sizeof(int));
    return tcp_fd;
}

int main(void)
{
    int unix_fd;
    int tcp_fd;
    int size;
    struct sockaddr_un un;
    char buf[256];

    unix_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (unix_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&un, 0, sizeof(un));
    un.sun_family = AF_UNIX;
    strncpy(un.sun_path, SOCKET_PATH, sizeof(un.sun_path) - 1);
    un.sun_path[sizeof(un.sun_path) - 1] = '\0';
    size = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);

    if (connect(unix_fd, (struct sockaddr *)&un, size) < 0) {
        perror("connect");
        close(unix_fd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to %s\n", SOCKET_PATH);

    tcp_fd = receive_fd(unix_fd);
    if (tcp_fd < 0) {
        close(unix_fd);
        exit(EXIT_FAILURE);
    }

    printf("Received TCP socket\n");
    close(unix_fd);
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        if (write(tcp_fd, buf, strlen(buf)) < 0) {
            perror("write");
            break;
        }
    }
    close(tcp_fd);
    return 0;
}