#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/pkt_sched.h>
#include <linux/pkt_cls.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <sys/socket.h>
#include <bpf/libbpf.h>
#include <linux/bpf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define MAX_MSG_SIZE 1024

static int addattr(struct nlmsghdr *n, int maxlen,
                   int type, const void *data, int len)
{
    int attr_len = RTA_LENGTH(len);

    if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(attr_len) > maxlen)
        return -1;

    struct rtattr *attr = (struct rtattr *)
        ((char *)n + NLMSG_ALIGN(n->nlmsg_len));

    attr->rta_type = type;
    attr->rta_len = attr_len;

    if (len)
        memcpy(RTA_DATA(attr), data, len);

    n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) +
                   RTA_ALIGN(attr_len);

    return 0;
}

int main(int argc, char *argv[])
{
    struct bpf_object *obj;
    struct bpf_program *prog;
    struct sockaddr_nl local = {0};
    struct sockaddr_nl kernel = {0};

    int fd, prog_fd;
    unsigned int ifindex;

    struct {
        struct nlmsghdr n;
        struct tcmsg t;
        char attrs[MAX_MSG_SIZE];
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

    /* Load the eBPF program */

    obj = bpf_object__open_file("tc_drop.bpf.o", NULL);
    if (libbpf_get_error(obj)) {
        fprintf(stderr, "bpf_object__open_file failed\n");
        return 1;
    }

    prog = bpf_object__find_program_by_name(obj, "tc_drop");
    if (!prog) {
        fprintf(stderr, "eBPF program not found\n");
        bpf_object__close(obj);
        return 1;
    }

    bpf_program__set_type(prog, BPF_PROG_TYPE_SCHED_CLS);

    if (bpf_object__load(obj) < 0) {
        fprintf(stderr, "bpf_object__load failed\n");
        bpf_object__close(obj);
        return 1;
    }

    prog_fd = bpf_program__fd(prog);

    /* Create a Netlink socket */

    fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0) {
        perror("socket");
        bpf_object__close(obj);
        return 1;
    }

    local.nl_family = AF_NETLINK;

    if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("bind");
        close(fd);
        bpf_object__close(obj);
        return 1;
    }

    /* Initialize the Netlink message */

    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
    req.n.nlmsg_type = RTM_NEWTFILTER;
    req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK |
                        NLM_F_CREATE | NLM_F_EXCL;
    req.n.nlmsg_seq = 1;

    req.t.tcm_family = AF_UNSPEC;
    req.t.tcm_ifindex = ifindex;
    req.t.tcm_parent =
        TC_H_MAKE(TC_H_CLSACT, TC_H_MIN_INGRESS);
    req.t.tcm_handle = 1;
    req.t.tcm_info = TC_H_MAKE(1 << 16, htons(ETH_P_ALL));

    /* Add the TCA_KIND attribute */

    if (addattr(&req.n, sizeof(req), TCA_KIND,
                "bpf", sizeof("bpf")) < 0) {
        fprintf(stderr, "Unable to add TCA_KIND\n");
        close(fd);
        bpf_object__close(obj);
        return 1;
    }

    /* Add the TCA_OPTIONS attribute and its nested attributes */

    struct rtattr *options = (struct rtattr *)
        ((char *)&req + NLMSG_ALIGN(req.n.nlmsg_len));

    if (addattr(&req.n, sizeof(req), TCA_OPTIONS, NULL, 0) < 0) {
        fprintf(stderr, "Unable to add TCA_OPTIONS\n");
        close(fd);
        bpf_object__close(obj);
        return 1;
    }

    int flags = TCA_BPF_FLAG_ACT_DIRECT;

    if (addattr(&req.n, sizeof(req), TCA_BPF_FD,
                &prog_fd, sizeof(prog_fd)) < 0 ||
        addattr(&req.n, sizeof(req), TCA_BPF_NAME,
                "tc_drop", sizeof("tc_drop")) < 0 ||
        addattr(&req.n, sizeof(req), TCA_BPF_FLAGS,
                &flags, sizeof(flags)) < 0) {
        fprintf(stderr, "Unable to add BPF attributes\n");
        close(fd);
        bpf_object__close(obj);
        return 1;
    }

    options->rta_len = (char *)&req + req.n.nlmsg_len -
                       (char *)options;

    /* Send the request to the kernel */

    kernel.nl_family = AF_NETLINK;

    if (sendto(fd, &req, req.n.nlmsg_len, 0,
               (struct sockaddr *)&kernel, sizeof(kernel)) < 0) {
        perror("sendto");
        close(fd);
        bpf_object__close(obj);
        return 1;
    }

    /* Receive the kernel's acknowledgment */

    char buffer[4096];
    int len = recv(fd, buffer, sizeof(buffer), 0);

    if (len < 0) {
        perror("recv");
        close(fd);
        bpf_object__close(obj);
        return 1;
    }

    struct nlmsghdr *nlh;

    for (nlh = (struct nlmsghdr *)buffer;
         NLMSG_OK(nlh, len);
         nlh = NLMSG_NEXT(nlh, len)) {

        if (nlh->nlmsg_type == NLMSG_ERROR) {
            struct nlmsgerr *err = NLMSG_DATA(nlh);

            if (err->error == 0) {
                printf("BPF filter attached to %s ingress\n",
                       argv[1]);
            } else {
                errno = -err->error;
                perror("Netlink");
            }

            close(fd);
            bpf_object__close(obj);
            return err->error ? 1 : 0;
        }
    }

    close(fd);
    bpf_object__close(obj);
    return 1;
}
