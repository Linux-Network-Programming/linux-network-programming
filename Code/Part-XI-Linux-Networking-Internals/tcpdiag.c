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

static const char *tcp_state(unsigned int state)
{
    static const char *states[] = {
        "UNKNOWN", "ESTABLISHED", "SYN_SENT", "SYN_RECV",
        "FIN_WAIT1", "FIN_WAIT2", "TIME_WAIT", "CLOSE",
        "CLOSE_WAIT", "LAST_ACK", "LISTEN", "CLOSING",
        "NEW_SYN_RECV"
    };

    if (state >= sizeof(states) / sizeof(states[0]))
        return "UNKNOWN";

    return states[state];
}

static void print_socket(struct inet_diag_msg *diag)
{
    char src[INET_ADDRSTRLEN];
    char dst[INET_ADDRSTRLEN];

    struct in_addr src_addr = {
        .s_addr = diag->id.idiag_src[0]
    };

    struct in_addr dst_addr = {
        .s_addr = diag->id.idiag_dst[0]
    };

    inet_ntop(AF_INET, &src_addr, src, sizeof(src));
    inet_ntop(AF_INET, &dst_addr, dst, sizeof(dst));

    printf("%-16s %-22s %-22s %8u %8u %8u %10u %10u\n",
           tcp_state(diag->idiag_state),
           src, dst,
           ntohs(diag->id.idiag_sport),
           ntohs(diag->id.idiag_dport),
           diag->idiag_uid,
           diag->idiag_rqueue,
           diag->idiag_wqueue);
}

int main(void)
{
    int sock;
    char buffer[BUFFER_SIZE];

    struct sockaddr_nl local = {0};
    struct sockaddr_nl kernel = {0};

    sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_SOCK_DIAG);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    local.nl_family = AF_NETLINK;
    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("bind");
        close(sock);
        exit(EXIT_FAILURE);
    }

    kernel.nl_family = AF_NETLINK;
    memset(buffer, 0, sizeof(buffer));

    struct nlmsghdr *nlh = (struct nlmsghdr *)buffer;
    nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct inet_diag_req_v2));
    nlh->nlmsg_type = SOCK_DIAG_BY_FAMILY;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    nlh->nlmsg_seq = 1;
    nlh->nlmsg_pid = getpid();

    struct inet_diag_req_v2 *req = NLMSG_DATA(nlh);
    req->sdiag_family = AF_INET;
    req->sdiag_protocol = IPPROTO_TCP;
    req->idiag_ext = 0;
    req->idiag_states = UINT32_MAX;

    if (sendto(sock, buffer, nlh->nlmsg_len, 0, (struct sockaddr *)&kernel, sizeof(kernel)) < 0) {
        perror("sendto");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("%-16s %-22s %-22s %8s %8s %s %10s %10s\n",
           "STATE", "LOCAL ADDRESS", "REMOTE ADDRESS",
           "LOCAL PORT", "REMOTE PORT", "UID",
           "RECV-Q", "SEND-Q");

    for (;;) {
        int len = recv(sock, buffer, sizeof(buffer), 0);

        if (len < 0) {
            if (errno == EINTR) continue;
            perror("recv");
            close(sock);
            exit(EXIT_FAILURE);
        }

        struct nlmsghdr *h = (struct nlmsghdr *)buffer;
        while (NLMSG_OK(h, len)) {
            if (h->nlmsg_type == NLMSG_DONE) {
                close(sock);
                return 0;
            }

            if (h->nlmsg_type == NLMSG_ERROR) {
                struct nlmsgerr *err = NLMSG_DATA(h);
                if (err->error != 0) {
                    fprintf(stderr, "Netlink error: %s\n", strerror(-err->error));
                    close(sock);
                    exit(EXIT_FAILURE);
                }
            } else if (h->nlmsg_type == SOCK_DIAG_BY_FAMILY) {
                struct inet_diag_msg *diag = NLMSG_DATA(h);
                print_socket(diag);
            }
            h = NLMSG_NEXT(h, len);
        }
    }
}
