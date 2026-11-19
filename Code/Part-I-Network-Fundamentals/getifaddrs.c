#include <arpa/inet.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <netpacket/packet.h>

int main()
{
    struct ifaddrs *ifaddr;
    char host[INET6_ADDRSTRLEN];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
    
        /* Display interface name, family and address */

        printf("Interface    : %-8s \nFamily       : %s (%d)\n", ifa->ifa_name,
            (ifa->ifa_addr->sa_family == AF_PACKET) ? "AF_PACKET" :
            (ifa->ifa_addr->sa_family == AF_INET) ? "AF_INET" :
            (ifa->ifa_addr->sa_family == AF_INET6) ? "AF_INET6" : "???",
            ifa->ifa_addr->sa_family);
          
        switch (ifa->ifa_addr->sa_family) {

        case AF_PACKET:
            struct sockaddr_ll *sll = (struct sockaddr_ll *)ifa->ifa_addr;
            printf("MAC address  : %02x:%02x:%02x:%02x:%02x:%02x\n",
                    sll->sll_addr[0], sll->sll_addr[1], sll->sll_addr[2],
                    sll->sll_addr[3], sll->sll_addr[4], sll->sll_addr[5]);
        break;

        case AF_INET:
            struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
            if (inet_ntop(AF_INET, &sin->sin_addr, host, sizeof(host)) == NULL) {
                perror("inet_ntop");
                continue;
            }
            printf("IPv4 Address : %s\n", host);
        break;

        case AF_INET6:
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
            if (inet_ntop(AF_INET6, &sin6->sin6_addr, host, sizeof(host)) == NULL) {
                perror("inet_ntop");
                continue;
            }
            printf("IPv6 Address : %s\n", host);
        break;
        
        }
        printf("\n");
    }

    freeifaddrs(ifaddr);
    exit(EXIT_SUCCESS);
}

