#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define SOCKET_PATH "/tmp/logger.sock"
#define LOG_SIZE    1024

int main(void)
{
    int sock;
    struct sockaddr_un addr;

    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_PASSCRED, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(sock);
        exit(EXIT_FAILURE);
    }

    unlink(SOCKET_PATH);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Logging daemon listening on %s\n", SOCKET_PATH);
    for (;;) {
        char buffer[LOG_SIZE];
        char control[CMSG_SPACE(sizeof(struct ucred))];

        struct iovec iov = {
            .iov_base = buffer,
            .iov_len  = sizeof(buffer) - 1
        };

        struct msghdr msg = {
            .msg_name       = NULL,
            .msg_namelen    = 0,
            .msg_iov        = &iov,
            .msg_iovlen     = 1,
            .msg_control    = control,
            .msg_controllen = sizeof(control),
            .msg_flags      = 0
        };

        ssize_t n;
        n = recvmsg(sock, &msg, 0);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("recvmsg");
            break;
        }
        buffer[n] = '\0';

        /* Examine the ancillary data */
        struct cmsghdr *cmsg;
        for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_CREDENTIALS) {

                struct ucred *cred;
                cred = (struct ucred *)CMSG_DATA(cmsg);
                printf("[PID=%d UID=%d GID=%d] %s\n", cred->pid,
                       cred->uid, cred->gid, buffer);
                break;
            }
        }
    }
    close(sock);
    unlink(SOCKET_PATH);
    return 0;
}