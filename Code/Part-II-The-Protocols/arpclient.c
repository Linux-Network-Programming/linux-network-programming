#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <netinet/if_ether.h>

int main (int argc, char *argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Usage: %s ip_address", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct ifaddrs *ifaddr, *ifa;
    unsigned char local_mac[ETH_ALEN];
    struct in_addr local_ip;
    char ip[INET_ADDRSTRLEN];
    int ifindex;
    char *ifname;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

        if (ifa->ifa_addr == NULL)
            continue;

        /* Select the desired interface */
        if (strcmp(ifa->ifa_name, "wlan0") != 0)
            continue;
        else    
            ifname = ifa->ifa_name;

        switch (ifa->ifa_addr->sa_family) {

        case AF_PACKET: {
            struct sockaddr_ll *sll = (struct sockaddr_ll *)ifa->ifa_addr;
            memcpy(local_mac, sll->sll_addr, ETH_ALEN);
            break;
        }

        case AF_INET: {
            struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
            local_ip = sin->sin_addr;
            inet_ntop(AF_INET, &local_ip, ip, sizeof(ip));
            break;
        }
        }
    }

    /* Retrieve the interface index */
    ifindex = if_nametoindex("wlan0");
    if (ifindex == 0) {
        perror("if_nametoindex");
        exit(EXIT_FAILURE);
    }

    /* Construct the frame */

    struct arp_frame {
	    struct ethhdr ethernet;
	    struct ether_arp arp;
    };
    unsigned char broadcast_mac[ETH_ALEN] = { 0xff,0xff,0xff,0xff,0xff,0xff };

    struct arp_frame frame;
    memcpy(frame.ethernet.h_dest, broadcast_mac, ETH_ALEN);
    memcpy(frame.ethernet.h_source, local_mac, ETH_ALEN);
    frame.ethernet.h_proto = htons(ETH_P_ARP);
    frame.arp.ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
    frame.arp.ea_hdr.ar_pro = htons(ETH_P_IP);
    frame.arp.ea_hdr.ar_hln = ETH_ALEN;
    frame.arp.ea_hdr.ar_pln = 4;
    frame.arp.ea_hdr.ar_op = htons(ARPOP_REQUEST);
    memcpy(frame.arp.arp_sha, local_mac, ETH_ALEN);
    memcpy(frame.arp.arp_spa, &local_ip, sizeof(local_ip));
    memset(frame.arp.arp_tha, 0, ETH_ALEN);
    struct in_addr target_ip;
    inet_pton(AF_INET, argv[1], &target_ip);
    memcpy(frame.arp.arp_tpa, &target_ip, sizeof(target_ip));

     /* Create a raw socket to transmit the request */
    int sockfd2 = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
    if (sockfd2 == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

     /* Prepare sockaddrr_ll for transmission */

    struct sockaddr_ll socket_address;
    memset(&socket_address, 0, sizeof(socket_address));

    socket_address.sll_family   = AF_PACKET;
    socket_address.sll_protocol = htons(ETH_P_ARP);
    socket_address.sll_ifindex  = ifindex;
    socket_address.sll_halen    = ETH_ALEN;

    memcpy(socket_address.sll_addr, broadcast_mac, ETH_ALEN);

    /* Send the ARP request */
    printf("\nARP Request: [%s][%d] [%02x:%02x:%02x:%02x:%02x:%02x] [%s] --> %s ", ifname, ifindex,
        local_mac[0], local_mac[1], local_mac[2], local_mac[3], local_mac[4], local_mac[5],
        ip, argv[1]); 

    ssize_t n = sendto(sockfd2, &frame, sizeof(frame), 0, (struct sockaddr *)&socket_address, sizeof(socket_address));
    if (n == -1)
        perror("sendto");

     /* Receive the ARP Reply */
    unsigned char bufreply[ETH_FRAME_LEN];

    /* Configure a receive timeout to avoid blocking indefinitely. */
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sockfd2, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* Receive ARP packets until the expected reply is received or
     * the receive timeout expires  */
    for (;;) {

        ssize_t nrec = recvfrom(sockfd2, bufreply, sizeof(bufreply), 0, NULL, NULL);
        if (nrec == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("No ARP Reply received (timeout).\n");
                break;
            }
            perror("recvfrom");
            exit(EXIT_FAILURE);
        }

        struct ethhdr *ethreply = (struct ethhdr *)bufreply;
        struct ether_arp *arpreply = (struct ether_arp *)(bufreply + sizeof(struct ethhdr));

        /* Ignore ARP Requests */
        if (ntohs(arpreply->ea_hdr.ar_op) != ARPOP_REPLY)
            continue;

        /* Check that the reply was sent by the target host */
        if (memcmp(arpreply->arp_spa, &target_ip, sizeof(target_ip)) != 0)
            continue;

        /* Check that the reply is addressed to the local host */
        if (memcmp(arpreply->arp_tpa, &local_ip, sizeof(local_ip)) != 0)
            continue;

        printf("\nReply from: [%d.%d.%d.%d] [%02x:%02x:%02x:%02x:%02x:%02x]\n", arpreply->arp_spa[0],
            arpreply->arp_spa[1], arpreply->arp_spa[2], arpreply->arp_spa[3],
            arpreply->arp_sha[0], arpreply->arp_sha[1], arpreply->arp_sha[2], 
            arpreply->arp_sha[3], arpreply->arp_sha[4], arpreply->arp_sha[5]);

        break;
    }

    freeifaddrs(ifaddr);
    exit(EXIT_SUCCESS);
}