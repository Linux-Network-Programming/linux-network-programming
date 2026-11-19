#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_address(int family, void *addr)
{
    char buf[INET6_ADDRSTRLEN];

    if (family == AF_INET)
        inet_ntop(AF_INET, addr, buf, sizeof(buf));
    else if (family == AF_INET6)
        inet_ntop(AF_INET6, addr, buf, sizeof(buf));
    else
        return;
    printf("%s", buf);
}

static void print_rtmsg(struct rtmsg *rtm)
{
    printf("--------------------------------\n");
    printf("Family : %s\n", rtm->rtm_family == AF_INET ? "IPv4" : "IPv6"); 
    printf("Destination length : %u\n", rtm->rtm_dst_len);
    printf("Source length      : %u\n", rtm->rtm_src_len);
    printf("Table              : %u\n", rtm->rtm_table);
    printf("Protocol           : %u\n", rtm->rtm_protocol);
    printf("Scope              : %u\n", rtm->rtm_scope);
    printf("Type               : %u\n", rtm->rtm_type);
    printf("Flags              : 0x%x\n", rtm->rtm_flags);
}

static void parse_route_attributes(struct rtmsg *rtm, struct rtattr *attr, int len)
{
    while (RTA_OK(attr, len)) {
        switch(attr->rta_type) {
        case RTA_DST:
            printf("Destination : ");
            print_address(rtm->rtm_family, RTA_DATA(attr));
            printf("\n");
            break;

        case RTA_GATEWAY:
            printf("Gateway     : ");
            print_address(rtm->rtm_family, RTA_DATA(attr));
            printf("\n");
            break;

        case RTA_OIF: 
            int index = *(int *)RTA_DATA(attr);
            char name[IF_NAMESIZE];
            if (if_indextoname(index, name))
                printf("Output IF   : %s\n", name);
            else
                printf("Output IF   : %d\n", index);
            break;

        case RTA_PRIORITY:
            printf("Priority    : %u\n", *(unsigned int *)RTA_DATA(attr));
            break;

        case RTA_TABLE:
            printf("Table attr  : %u\n", *(unsigned int *)RTA_DATA(attr));
            break;

        case RTA_PREFSRC:
            printf("Preferred source : ");
            print_address(rtm->rtm_family, RTA_DATA(attr));
            printf("\n");
            break;

        default:
            printf("Attribute %d length %d\n", attr->rta_type, attr->rta_len);
        }

        attr = RTA_NEXT(attr, len);
    }
}