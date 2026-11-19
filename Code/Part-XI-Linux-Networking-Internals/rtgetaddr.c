#include "rtgetaddr.h"

int main(void)
{
    int sock;
    char buffer[BUFFER_SIZE];
    struct sockaddr_nl local = {0};
    struct sockaddr_nl kernel = {0};

    sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    local.nl_family = AF_NETLINK;
    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }

    kernel.nl_family = AF_NETLINK;
    struct {
        struct nlmsghdr nlh;
        struct ifaddrmsg ifa;
    } request;

    memset(&request, 0, sizeof(request));
    request.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    request.nlh.nlmsg_type = RTM_GETADDR;
    request.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    request.nlh.nlmsg_seq = 1;
    request.ifa.ifa_family = AF_UNSPEC;

    if (sendto(sock, &request, request.nlh.nlmsg_len, 0, (struct sockaddr *)&kernel, sizeof(kernel)) < 0) {
        perror("sendto");
        close(sock);
        return 1;
    }

    while (1) {
        ssize_t len = recv(sock, buffer, sizeof(buffer), 0);
        if (len < 0) {
            perror("recv");
            break;
        }

        struct nlmsghdr *nlh = (struct nlmsghdr *)buffer;
        while (NLMSG_OK(nlh, len)) {
            if (nlh->nlmsg_type == NLMSG_DONE)
                goto done;

            if (nlh->nlmsg_type == NLMSG_ERROR) {
                struct nlmsgerr *err = NLMSG_DATA(nlh);
                if (err->error) {
                    fprintf(stderr, "Netlink error: %d\n", err->error);
                goto done; }
            }

            if (nlh->nlmsg_type == RTM_NEWADDR) {
                struct ifaddrmsg *ifa = NLMSG_DATA(nlh);
                print_ifaddrmsg(ifa);
                int attrlen = IFA_PAYLOAD(nlh);
                struct rtattr *attr = IFA_RTA(ifa);
                parse_nic_attributes(ifa, attr, attrlen);
            }
            nlh = NLMSG_NEXT(nlh, len);
        }
    }

done:
    close(sock);
    return 0;
}