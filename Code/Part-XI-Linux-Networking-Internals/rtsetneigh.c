#include "rtsetnic.h"
#include <linux/neighbour.h>
#include <netinet/ether.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s ip-address mac-address nic\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    struct in_addr ip;
    if (inet_pton(AF_INET, argv[1], &ip) != 1) {
        fprintf(stderr, "Invalid IP address\n");
        exit(EXIT_FAILURE);
    }

    struct ether_addr mac;
    if (ether_aton_r(argv[2], &mac) == NULL) {
        fprintf(stderr, "Invalid MAC address\n");
        exit(EXIT_FAILURE);
    }

    unsigned int ifindex = if_nametoindex(argv[3]);
    if (ifindex == 0) {
        fprintf(stderr, "Interface %s not found\n", argv[3]);
        exit(EXIT_FAILURE);
    }

    int sock;
    char buffer[BUFFER_SIZE];
    struct sockaddr_nl local = {0};
    struct sockaddr_nl kernel = {0};

    sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
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
    nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct ndmsg));
    nlh->nlmsg_type = RTM_NEWNEIGH;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK |
                       NLM_F_CREATE | NLM_F_EXCL;
    nlh->nlmsg_seq = 1;
    nlh->nlmsg_pid = getpid();

    struct ndmsg *ndm = NLMSG_DATA(nlh);
    ndm->ndm_family = AF_INET;
    ndm->ndm_ifindex = ifindex;
    ndm->ndm_state = NUD_PERMANENT;
    ndm->ndm_flags = 0;
    ndm->ndm_type = RTN_UNICAST;

    /* Add the destination IP and link-layer address attributes. */
    if (addattr(nlh, sizeof(buffer), NDA_DST, &ip, sizeof(ip)) < 0
        || addattr(nlh, sizeof(buffer), NDA_LLADDR, &mac, sizeof(mac)) < 0) {
        fprintf(stderr, "Cannot add neighbor attributes\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    if (sendto(sock, buffer, nlh->nlmsg_len, 0, (struct sockaddr *)&kernel, sizeof(kernel)) < 0) {
        perror("sendto");
        close(sock);
        exit(EXIT_FAILURE);
    }

    int len = recv(sock, buffer, sizeof(buffer), 0);
    if (len < 0) {
        perror("recv");
        close(sock);
        exit(EXIT_FAILURE);
    }

    nlh = (struct nlmsghdr *)buffer;
    while (NLMSG_OK(nlh, len)) {
        if (nlh->nlmsg_type == NLMSG_ERROR) {
            struct nlmsgerr *err = NLMSG_DATA(nlh);

            if (err->error == 0) {
                printf("Neighbor entry added successfully\n");
            } else {
                fprintf(stderr, "Netlink error: %s\n",
                        strerror(-err->error));
                close(sock);
                exit(EXIT_FAILURE);
            }
            break;
        }
        nlh = NLMSG_NEXT(nlh, len);
    }
    close(sock);
    return 0;
}
