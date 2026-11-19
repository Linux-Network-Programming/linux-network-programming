#include <arpa/inet.h>
#include <errno.h>
#include <linux/netlink.h>
#include <linux/netfilter.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nf_tables.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_nl local, kernel;
    struct nlmsghdr *nlh;
    struct nfgenmsg *nfg;
    struct nlattr *attr;
    unsigned char buffer[BUFFER_SIZE];
    unsigned int seq = 1;
    ssize_t n;
    int offset, attr_len;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <table_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    sockfd = socket(AF_NETLINK, SOCK_RAW, NETLINK_NETFILTER);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&local, 0, sizeof(local));
    local.nl_family = AF_NETLINK;
    if (bind(sockfd, (struct sockaddr *)&local, sizeof(local)) == -1) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    memset(&kernel, 0, sizeof(kernel));
    kernel.nl_family = AF_NETLINK;
    kernel.nl_pid = 0;

    memset(buffer, 0, sizeof(buffer));

    nlh = (struct nlmsghdr *)buffer;
    nlh->nlmsg_type = (NFNL_SUBSYS_NFTABLES << 8) | NFT_MSG_GETTABLE;
    nlh->nlmsg_flags = NLM_F_REQUEST;
    nlh->nlmsg_seq = seq;
    nlh->nlmsg_pid = local.nl_pid;

    nfg = (struct nfgenmsg *)NLMSG_DATA(nlh);
    nfg->nfgen_family = NFPROTO_INET;
    nfg->version = NFNETLINK_V0;
    nfg->res_id = 0;

    offset = NLMSG_HDRLEN + NLMSG_ALIGN(sizeof(*nfg));

    attr = (struct nlattr *)(buffer + offset);
    attr_len = NLA_HDRLEN + strlen(argv[1]) + 1;
    attr->nla_type = NFTA_TABLE_NAME;
    attr->nla_len = attr_len;
    memcpy((char *)attr + NLA_HDRLEN, argv[1], strlen(argv[1]) + 1);
    nlh->nlmsg_len = offset + NLA_ALIGN(attr_len);

    if (sendto(sockfd, buffer, nlh->nlmsg_len, 0, (struct sockaddr *)&kernel, sizeof(kernel)) == -1) {
        perror("sendto");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    /* Receive and process the kernel response */
    for (;;) {
        n = recv(sockfd, buffer, sizeof(buffer), 0);
        if (n == -1) {
            perror("recv");
            break;
        }

        int msg_remaining = n, attr_remaining;
        
        for (nlh = (struct nlmsghdr *)buffer;
             NLMSG_OK(nlh, msg_remaining);
             nlh = NLMSG_NEXT(nlh, msg_remaining)) {

            if (nlh->nlmsg_seq != seq) continue;
            if (nlh->nlmsg_type == NLMSG_ERROR) {
                struct nlmsgerr *err = NLMSG_DATA(nlh);
                if (err->error == 0) continue;
                errno = -err->error;
                perror("Netlink");
                close(sockfd);
                exit(EXIT_FAILURE);
            }

            if (nlh->nlmsg_type == NLMSG_DONE) break;
            if (nlh->nlmsg_type != ((NFNL_SUBSYS_NFTABLES << 8) | NFT_MSG_NEWTABLE))
                continue;

            nfg = (struct nfgenmsg *)NLMSG_DATA(nlh);
            if (nfg->nfgen_family != NFPROTO_INET)
                continue;

            /* Parse the table attributes */
            offset = NLMSG_HDRLEN + NLMSG_ALIGN(sizeof(*nfg));
            attr_remaining = nlh->nlmsg_len - offset;
            attr = (struct nlattr *)((char *)nlh + offset);

            while (attr_remaining >= NLA_HDRLEN) {
                if (attr->nla_len < NLA_HDRLEN || attr->nla_len > attr_remaining)
                    break;

                if ((attr->nla_type & NLA_TYPE_MASK) == NFTA_TABLE_NAME) {
                    size_t len = attr->nla_len - NLA_HDRLEN;
                    char name[256];
                    if (len == 0 || len > sizeof(name) || memchr((char *)attr + NLA_HDRLEN, '\0', len) == NULL)
                        break;

                    memcpy(name, (char *)attr + NLA_HDRLEN, len);
                    printf("Table: %s\n", name);
                    close(sockfd);
                    return 0;
                }

                int step = NLA_ALIGN(attr->nla_len);
                if (step > attr_remaining) break;
                attr_remaining -= step;
                attr = (struct nlattr *)((char *)attr + step);
            }
        }
    }
    close(sockfd);
    return EXIT_FAILURE;
}