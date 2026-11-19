#include "rtsetnic.h"

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s new|del nic\n", argv[0]);
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
    struct ifinfomsg *ifi;

    nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));

    if (strcmp(argv[1], "new") == 0) {
        nlh->nlmsg_type = RTM_NEWLINK;
        nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_EXCL;
    } else if (strcmp(argv[1], "del") == 0) {
        nlh->nlmsg_type = RTM_DELLINK;
        nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    } else {
        fprintf(stderr, "Unknown command\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    nlh->nlmsg_seq = 1;
    nlh->nlmsg_pid = getpid();
    ifi = NLMSG_DATA(nlh);
    ifi->ifi_family = AF_UNSPEC;

    if (nlh->nlmsg_type == RTM_NEWLINK) {
        /* Specify the interface name. */
        if (addattr(nlh, sizeof(buffer), IFLA_IFNAME, argv[2], strlen(argv[2]) + 1) < 0) {
            fprintf(stderr, "Cannot add IFLA_IFNAME\n");
            close(sock);
            exit(EXIT_FAILURE);
        }

        /* Add the nested IFLA_LINKINFO attribute. */
        size_t offset = NLMSG_ALIGN(nlh->nlmsg_len);

        if (addattr(nlh, sizeof(buffer), IFLA_LINKINFO, NULL, 0) < 0) {
            fprintf(stderr, "Cannot add IFLA_LINKINFO\n");
            close(sock);
            exit(EXIT_FAILURE);
        }

        struct rtattr *linkinfo = (struct rtattr *)((char *)nlh + offset);

        /* Add the nested IFLA_INFO_KIND attribute. */
        struct rtattr *kind = (struct rtattr *)((char *)linkinfo + RTA_LENGTH(0));
        kind->rta_type = IFLA_INFO_KIND;
        kind->rta_len = RTA_LENGTH(sizeof("dummy"));

        memcpy(RTA_DATA(kind), "dummy", sizeof("dummy"));
        linkinfo->rta_len += RTA_ALIGN(kind->rta_len);
        nlh->nlmsg_len += RTA_ALIGN(kind->rta_len);

    } else {
        ifi->ifi_index = if_nametoindex(argv[2]);
        if (ifi->ifi_index == 0) {
            fprintf(stderr, "Interface %s not found\n", argv[2]);
            close(sock);
            exit(EXIT_FAILURE);
        }
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

        if (nlh->nlmsg_type == NLMSG_DONE)
            break;

        if (nlh->nlmsg_type == NLMSG_ERROR) {
            struct nlmsgerr *err = NLMSG_DATA(nlh);
            if (err->error == 0) {
                printf("Operation completed successfully\n");
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