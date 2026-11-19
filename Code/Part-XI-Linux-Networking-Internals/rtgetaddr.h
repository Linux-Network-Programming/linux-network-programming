#include <arpa/inet.h>
#include <linux/if_addr.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 8192

static void print_ipv4(const void *addr)
{
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, addr, buf, sizeof(buf));
    printf("%s", buf);
}

static void print_ipv6(const void *addr)
{
    char buf[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, addr, buf, sizeof(buf));
    printf("%s", buf);
}

static void print_address(int family, const void *addr)
{
    if (family == AF_INET)
        print_ipv4(addr);
    else if (family == AF_INET6)
        print_ipv6(addr);
}

static void parse_nic_attributes(struct ifaddrmsg *ifa, struct rtattr *attr, int len) 
{
  while (RTA_OK(attr, len)) {  
        switch (attr->rta_type) {
            case IFA_ADDRESS:
                printf("IFA_ADDRESS   : ");
                print_address(ifa->ifa_family, RTA_DATA(attr));
                printf("\n");
                break;

            case IFA_LOCAL: 
                printf("IFA_LOCAL     : ");
                print_address(ifa->ifa_family, RTA_DATA(attr));
                printf("\n");
                break;

            case IFA_BROADCAST:
                printf("IFA_BROADCAST : ");
                print_address(ifa->ifa_family, RTA_DATA(attr));
                printf("\n");
                break;

            case IFA_LABEL:
                printf("IFA_LABEL     : %s\n", (char *)RTA_DATA(attr));
                break;

            case IFA_CACHEINFO: {
                struct ifa_cacheinfo *ci = RTA_DATA(attr);
                printf("IFA_CACHEINFO\n");
                printf("   preferred : %u sec\n", ci->ifa_prefered);
                printf("   valid     : %u sec\n", ci->ifa_valid);
                printf("   created   : %u cs\n",  ci->cstamp);
                printf("   updated   : %u cs\n",  ci->tstamp);
                break;
            }

            case IFA_FLAGS: {
                unsigned int flags =  *(unsigned int *)RTA_DATA(attr);
                printf("IFA_FLAGS     : 0x%x\n", flags);
                break;
            }

            default:
                printf("Attribute %d (length %d)\n", attr->rta_type, attr->rta_len);
        }
        attr = RTA_NEXT(attr, len);
    }
}

void print_ifaddrmsg(struct ifaddrmsg *ifa) {
    printf("-------------------------------------\n");
    char ifname[IF_NAMESIZE];

    if (if_indextoname(ifa->ifa_index, ifname))
        printf("Interface : %d (%s)\n", ifa->ifa_index, ifname);
    else 
        printf("Interface : %d\n", ifa->ifa_index);
    
    printf("Family    : %s\n", ifa->ifa_family == AF_INET ? "IPv4" 
                : ifa->ifa_family == AF_INET6 ? "IPv6" : "Unknown");
    
    printf("Prefix    : %u\n", ifa->ifa_prefixlen);
    printf("Flags     : 0x%x\n", ifa->ifa_flags);
    printf("Scope     : %u\n", ifa->ifa_scope);

}