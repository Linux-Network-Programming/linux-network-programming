#include "rtsetnic.h"
#include <errno.h>

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s destination/prefix gateway nic\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Parse the destination network and prefix length. */
    char *slash = strchr(argv[1], '/');
    if (slash == NULL) {
        fprintf(stderr, "Invalid destination format\n");
        exit(EXIT_FAILURE);
    }

    *slash++ = '\0';
    char *end;
    errno = 0;
    unsigned long prefix = strtoul(slash, &end, 10);

    if (errno || end == slash || *end != '\0' || prefix > 32) {
        fprintf(stderr, "Invalid prefix length\n");
        exit(EXIT_FAILURE);
    }

    struct in_addr dst, gateway;
    if (inet_pton(AF_INET, argv[1], &dst) != 1) {
        fprintf(stderr, "Invalid destination address\n");
        exit(EXIT_FAILURE);
    }

    if (inet_pton(AF_INET, argv[2], &gateway) != 1) {
        fprintf(stderr, "Invalid gateway address\n");
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
    nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    nlh->nlmsg_type = RTM_NEWROUTE;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_EXCL;
    nlh->nlmsg_seq = 1;
    nlh->nlmsg_pid = getpid();

    struct rtmsg *rtm = NLMSG_DATA(nlh);
    rtm->rtm_family = AF_INET;
    rtm->rtm_dst_len = prefix;
    rtm->rtm_table = RT_TABLE_MAIN;
    rtm->rtm_protocol = RTPROT_STATIC;
    rtm->rtm_scope = RT_SCOPE_UNIVERSE;
    rtm->rtm_type = RTN_UNICAST;

    /* Add the destination, gateway, and output interface attributes. */
    if (addattr(nlh, sizeof(buffer), RTA_DST, &dst, sizeof(dst)) < 0 ||
        addattr(nlh, sizeof(buffer), RTA_GATEWAY, &gateway, sizeof(gateway)) < 0 ||
        addattr(nlh, sizeof(buffer), RTA_OIF, &ifindex, sizeof(ifindex)) < 0) {
        fprintf(stderr, "Cannot add route attributes\n");
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
                printf("Route added successfully\n");
            } else {
                fprintf(stderr, "Netlink error: %s\n", strerror(-err->error));
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