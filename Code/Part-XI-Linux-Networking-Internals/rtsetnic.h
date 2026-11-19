#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if_link.h>
#include <net/if.h>
#include  <arpa/inet.h>

#define BUFFER_SIZE 8192

static int addattr(struct nlmsghdr *nlh, size_t maxlen, unsigned short type, const void *data,
                   size_t data_len)
{
    size_t len = RTA_LENGTH(data_len);
    size_t offset = NLMSG_ALIGN(nlh->nlmsg_len);

    if (offset + RTA_ALIGN(len) > maxlen)
        return -1;

    struct rtattr *rta = (struct rtattr *) ((char *)nlh + offset);

    rta->rta_type = type;
    rta->rta_len = len;

    if (data_len > 0)
        memcpy(RTA_DATA(rta), data, data_len);

    nlh->nlmsg_len = offset + RTA_ALIGN(len);
    return 0;
}