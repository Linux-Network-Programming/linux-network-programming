#include <arpa/inet.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <linux/netlink.h>
#include <linux/sock_diag.h>
#include <linux/unix_diag.h>
#include <linux/rtnetlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 8192

static const char *socket_type_name(unsigned int type)
{
    switch (type) {
    case SOCK_STREAM:    return "STREAM";
    case SOCK_DGRAM:     return "DGRAM";
    case SOCK_SEQPACKET: return "SEQPACKET";
    default:             return "UNKNOWN";
    }
}

static const char *socket_state_name(unsigned int state)
{
    switch (state) {
    case TCP_ESTABLISHED: return "ESTABLISHED";
    case TCP_LISTEN:      return "LISTEN";
    default:              return "OTHER";
    }
}

static void print_socket_name(const unsigned char *name, int len)
{
    if (len <= 0) {
        printf("-");
        return;
    }

    /* Abstract UNIX domain socket names start with a null byte. */
    if (name[0] == '\0') {
        printf("@");
        name++;
        len--;
    } else if (name[len - 1] == '\0') {
        /* Ignore the terminating null byte of a pathname. */
        len--;
    }

    if (len == 0) return;
    for (int i = 0; i < len; i++) {
        if (name[i] >= 32 && name[i] <= 126)
            putchar(name[i]);
        else
            printf("\\x%02x", name[i]);
    }
}

static void process_message(struct nlmsghdr *nlh)
{
    struct unix_diag_msg *udiag;
    struct rtattr *attr;
    int attrlen;

    udiag = (struct unix_diag_msg *) NLMSG_DATA(nlh);

    printf("%-10u %-10s %-12s ", udiag->udiag_ino,
           socket_type_name(udiag->udiag_type),
           socket_state_name(udiag->udiag_state));

    printf("%-30s ", "");
    printf("%-10s ", "-");
    printf("%-10s\n", "-");

    attr = (struct rtattr *)((char *) udiag + NLMSG_ALIGN(sizeof(*udiag)));
    attrlen = nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*udiag));

    while (RTA_OK(attr, attrlen)) {
        switch (attr->rta_type) {
        case UNIX_DIAG_NAME:
            printf("    Name: ");
            print_socket_name((unsigned char *) RTA_DATA(attr), RTA_PAYLOAD(attr));
            printf("\n");
            break;

        case UNIX_DIAG_PEER:
            if (RTA_PAYLOAD(attr) >= sizeof(__u32)) {
                __u32 peer;
                memcpy(&peer, RTA_DATA(attr), sizeof(peer));
                printf("    Peer inode: %u\n", peer);
            }
            break;

        case UNIX_DIAG_RQLEN:
            if (RTA_PAYLOAD(attr) >=
                sizeof(struct unix_diag_rqlen)) {
                struct unix_diag_rqlen *qlen = (struct unix_diag_rqlen *) RTA_DATA(attr);
                printf("    Receive queue: %u\n", qlen->udiag_rqueue);
                printf("    Write queue:   %u\n", qlen->udiag_wqueue);
            }
            break;
        }
        attr = RTA_NEXT(attr, attrlen);
    }

    printf("\n");
}