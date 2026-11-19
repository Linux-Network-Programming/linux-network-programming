#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>

#define BUFFER_SIZE 8192

static void parse_nic_attributes(struct rtattr *attr, int len)
{
    while (RTA_OK(attr, len)) {
        switch (attr->rta_type) {

        case IFLA_IFNAME:
            printf("Name          : %s\n", (char *)RTA_DATA(attr));
            break;
        case IFLA_MTU:
            printf("MTU           : %u\n", *(unsigned int *)RTA_DATA(attr));
            break;
        case IFLA_ADDRESS: {
            unsigned char *mac = RTA_DATA(attr);
            printf("MAC address   : ");
            for (int i = 0; i < RTA_PAYLOAD(attr); i++) {
                printf("%02x", mac[i]);
                if (i != RTA_PAYLOAD(attr) - 1)
                    printf(":");
            }
            printf("\n");
            break;
        }
        default:
            break;
        }
        attr = RTA_NEXT(attr, len);
    }
}