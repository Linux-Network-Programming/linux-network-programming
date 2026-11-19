#include "rtnetlinkhead.h"

int main(void)
{
    int sock;
    struct sockaddr_nl addr;
    char buffer[BUFFER_SIZE];

    sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    memset(buffer, 0, sizeof(buffer));
    struct nlmsghdr *nlh = (struct nlmsghdr *)buffer;
    struct ifinfomsg *ifi;

    nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    nlh->nlmsg_type = RTM_GETLINK;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    nlh->nlmsg_seq = 1;
    nlh->nlmsg_pid = getpid();

    ifi = NLMSG_DATA(nlh);
    ifi->ifi_family = AF_UNSPEC;

    if (send(sock, buffer, nlh->nlmsg_len, 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    int done = 0;
    while (!done) {

        int len = recv(sock, buffer, sizeof(buffer), 0);
        if (len < 0) {
            perror("recv");
            exit(EXIT_FAILURE);
        }

        nlh = (struct nlmsghdr *)buffer;
        while (NLMSG_OK(nlh, len)) {

            if (nlh->nlmsg_type == NLMSG_DONE) {
                done = 1;
                break;
            }

            if (nlh->nlmsg_type == NLMSG_ERROR) {
                struct nlmsgerr *err = NLMSG_DATA(nlh);
                fprintf(stderr, "Netlink error: %d\n", err->error);
                exit(EXIT_FAILURE);
            }

            if (nlh->nlmsg_type == RTM_NEWLINK) {
                ifi = NLMSG_DATA(nlh);
                printf("---------------------------------\n");
                printf("Index         : %d\n", ifi->ifi_index);
                printf("Flags         : 0x%x\n", ifi->ifi_flags);

                int attrlen = nlh->nlmsg_len - NLMSG_LENGTH(sizeof(struct ifinfomsg));
                struct rtattr *attr = IFLA_RTA(ifi);
                parse_nic_attributes(attr, attrlen);
            }
            nlh = NLMSG_NEXT(nlh, len);
        }
    }
    close(sock);
    return 0;
}