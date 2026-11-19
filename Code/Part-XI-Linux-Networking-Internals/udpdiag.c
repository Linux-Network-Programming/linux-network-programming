#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/sock_diag.h>
#include <linux/inet_diag.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#define BUFFER_SIZE 32768

static void print_socket(struct inet_diag_msg *diag)
{
    char src[INET_ADDRSTRLEN];
    char dst[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &diag->id.idiag_src[0], src, sizeof(src));
    inet_ntop(AF_INET, &diag->id.idiag_dst[0], dst, sizeof(dst));

    printf("%-18s %-18s %10u %10u %6u %10u %10u\n",
           src, dst, ntohs(diag->id.idiag_sport), ntohs(diag->id.idiag_dport),
           diag->idiag_uid, diag->idiag_rqueue, diag->idiag_wqueue);
}

int main(void)
{
    int fd;
    struct sockaddr_nl local = {0};
    struct sockaddr_nl kernel = {0};
    struct nlmsghdr *nlh;
    struct inet_diag_req_v2 req = {0};
    char buffer[BUFFER_SIZE];
    ssize_t len;

    fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_SOCK_DIAG);
    if (fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    local.nl_family = AF_NETLINK;
    if (bind(fd, (struct sockaddr *)&local, sizeof(local)) == -1) {
        perror("bind");
        close(fd);
        exit(EXIT_FAILURE);
    }

    kernel.nl_family = AF_NETLINK;
    nlh = (struct nlmsghdr *)buffer;
    memset(buffer, 0, sizeof(buffer));

    nlh->nlmsg_len = NLMSG_LENGTH(sizeof(req));
    nlh->nlmsg_type = SOCK_DIAG_BY_FAMILY;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    nlh->nlmsg_seq = 1;
    nlh->nlmsg_pid = getpid();

    req.sdiag_family = AF_INET;
    req.sdiag_protocol = IPPROTO_UDP;
    req.idiag_ext = 0;
    req.idiag_states = UINT32_MAX;

    memcpy(NLMSG_DATA(nlh), &req, sizeof(req));

    if (sendto(fd, buffer, nlh->nlmsg_len, 0, (struct sockaddr *)&kernel, sizeof(kernel)) == -1) {
        perror("sendto");
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("%-18s %-18s %10s %10s %6s %10s %10s\n",
           "LOCAL ADDRESS", "REMOTE ADDRESS",
           "LOCAL PORT", "REMOTE PORT", "UID",
           "RECV-Q", "SEND-Q");

    while ((len = recv(fd, buffer, sizeof(buffer), 0)) > 0) {
        struct nlmsghdr *msg;
        if (len == -1) {
            if (errno == EINTR) continue;
            perror("recv");
            break;
        }

        for (msg = (struct nlmsghdr *)buffer;
             NLMSG_OK(msg, (unsigned int)len);
             msg = NLMSG_NEXT(msg, len)) {

            if (msg->nlmsg_type == NLMSG_DONE) {
                close(fd);
                return 0;
            }

            if (msg->nlmsg_type == NLMSG_ERROR) {
                struct nlmsgerr *err = NLMSG_DATA(msg);

                if (err->error != 0)
                    fprintf(stderr, "Netlink error: %s\n", strerror(-err->error));
                close(fd);
                return err->error ? EXIT_FAILURE : 0;
            }

            if (msg->nlmsg_type == SOCK_DIAG_BY_FAMILY) {
                struct inet_diag_msg *diag = NLMSG_DATA(msg);
                print_socket(diag);
            }
        }
    }
    if (len == -1) perror("recv");
    close(fd);
    return EXIT_SUCCESS;
}
