#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/pkt_sched.h>
#include <net/if.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    int fd;
    unsigned int ifindex;
    struct sockaddr_nl local = {0};
    struct sockaddr_nl kernel = {0};

    struct {
        struct nlmsghdr n;
        struct tcmsg t;
        char attrs[256];
    } req = {0};

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <interface>\n", argv[0]);
        return 1;
    }

    ifindex = if_nametoindex(argv[1]);
    if (ifindex == 0) {
        perror("if_nametoindex");
        return 1;
    }

    /* Create a Netlink socket. */
    fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    /* Bind the socket to a local Netlink address. */
    local.nl_family = AF_NETLINK;
    if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("bind");
        close(fd);
        return 1;
    }

    socklen_t addrlen = sizeof(local);
    if (getsockname(fd, (struct sockaddr *)&local, &addrlen) < 0) {
        perror("getsockname");
        close(fd);
        return 1;
    }

    /* Initialize the Netlink message. */
    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
    req.n.nlmsg_type = RTM_NEWQDISC;
    req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK |
                        NLM_F_CREATE | NLM_F_EXCL;
    req.n.nlmsg_seq = 1;
    req.n.nlmsg_pid = local.nl_pid;

    /* Initialize the traffic control message. */
    req.t.tcm_family = AF_UNSPEC;
    req.t.tcm_ifindex = ifindex;
    req.t.tcm_handle = TC_H_MAKE(TC_H_CLSACT, 0);
    req.t.tcm_parent = TC_H_CLSACT;

    /* Add the TCA_KIND attribute. */
    struct rtattr *attr;
    attr = (struct rtattr *)((char *)&req + NLMSG_ALIGN(req.n.nlmsg_len));
    attr->rta_type = TCA_KIND;
    attr->rta_len = RTA_LENGTH(sizeof("clsact"));
    memcpy(RTA_DATA(attr), "clsact", sizeof("clsact"));
    req.n.nlmsg_len = NLMSG_ALIGN(req.n.nlmsg_len) + RTA_ALIGN(attr->rta_len);

    /* Send the request to the kernel. */
    kernel.nl_family = AF_NETLINK;
    if (sendto(fd, &req, req.n.nlmsg_len, 0, (struct sockaddr *)&kernel, sizeof(kernel)) < 0) {
        perror("sendto");
        close(fd);
        return 1;
    }

    /* Receive the kernel's acknowledgment. */
    char buffer[4096];
    struct nlmsghdr *nlh;
    int len;

    len = recv(fd, buffer, sizeof(buffer), 0);
    if (len < 0) {
        perror("recv");
        close(fd);
        return 1;
    }

    for (nlh = (struct nlmsghdr *)buffer; NLMSG_OK(nlh, len); nlh = NLMSG_NEXT(nlh, len)) {
        if (nlh->nlmsg_type == NLMSG_ERROR) {
            struct nlmsgerr *err = NLMSG_DATA(nlh);
            if (err->error == 0) {
                printf("clsact qdisc created on %s\n", argv[1]);
            } else {
                errno = -err->error;
                perror("Netlink");
            }
            close(fd);
            return err->error ? 1 : 0;
        }
    }

    close(fd);
    return 1;
}