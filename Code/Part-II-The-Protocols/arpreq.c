#include <stdio.h>
#include <stdlib.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <errno.h>


int main (int argc, char *argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Usage: %s ip_address", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Create the socket to retrieve information */
    struct ifreq ifr;
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);  
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "wlan0", IFNAMSIZ -1);

    /* Retrieve the local MAC */
    unsigned char *mac; 
    unsigned char local_mac[ETH_ALEN];   

    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == -1) {
        perror("SIOCGIFHWADDR") ;
        exit(EXIT_FAILURE);
    }
    mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
    printf("Local MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    memcpy(local_mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

    /* Retrieve the NIC index */
    int ifindex;
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) == -1) {
        perror("SIOCGIFINDEX");
        exit(EXIT_FAILURE);
    }

    ifindex = ifr.ifr_ifindex;
    printf("Interface index: %d\n", ifr.ifr_ifindex);

   /* Retrieve the local IPv4 address */
    char ip[INET_ADDRSTRLEN];
    struct sockaddr_in *ipaddr;
    struct in_addr local_ip;

    if (ioctl(sockfd, SIOCGIFADDR, &ifr) == -1) {
        perror("SIOCGIFADDR");
        exit(EXIT_FAILURE);
    }
    printf("sa_family = %d\n", ifr.ifr_addr.sa_family);

    ipaddr = (struct sockaddr_in *)&ifr.ifr_addr;
    local_ip = ipaddr->sin_addr;

    if (inet_ntop(AF_INET, &local_ip, ip, sizeof(ip)) == NULL) {
        perror("inet_ntop");
        exit(EXIT_FAILURE);
    }
    printf("Local IP: %s\n", ip);


    /* Construct the Ethernet header */
    unsigned char buffer[ETH_FRAME_LEN];
    unsigned char broadcast_mac[ETH_ALEN] = { 0xff,0xff,0xff,0xff,0xff,0xff };
    struct ethhdr *eth = (struct ethhdr *)buffer;

    memcpy(eth->h_dest, broadcast_mac, ETH_ALEN);
    memcpy(eth->h_source, local_mac, ETH_ALEN);
    eth->h_proto = htons(ETH_P_ARP);

    /* Construct the ARP header */

    struct ether_arp *arp = (struct ether_arp *)(buffer + sizeof(struct ethhdr));
    
    arp->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);   /* Ethernet */
    arp->ea_hdr.ar_pro = htons(ETH_P_IP);       /* IPv4 */
    arp->ea_hdr.ar_hln = ETH_ALEN;              /* MAC length = 6 */
    arp->ea_hdr.ar_pln = 4;                     /* IPv4 length = 4 */
    arp->ea_hdr.ar_op  = htons(ARPOP_REQUEST);  /* ARP Request */

    memcpy(arp->arp_sha, local_mac, ETH_ALEN);
    memcpy(arp->arp_spa, &local_ip, sizeof(local_ip));
    memset(arp->arp_tha, 0, ETH_ALEN);

    struct in_addr target_ip;
    inet_pton(AF_INET, argv[1], &target_ip);
    memcpy(arp->arp_tpa, &target_ip, 4);


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

    /* Print NIC, index and source MAC  */

    printf("\nifr_name   = %s\n", ifr.ifr_name);
    printf("ifindex    = %d\n", ifindex);
    printf("Ether src = %02x:%02x:%02x:%02x:%02x:%02x\n", eth->h_source[0], eth->h_source[1], eth->h_source[2],
            eth->h_source[3], eth->h_source[4], eth->h_source[5]);

    printf("ARP SHA   = %02x:%02x:%02x:%02x:%02x:%02x\n", arp->arp_sha[0], arp->arp_sha[1], arp->arp_sha[2],
            arp->arp_sha[3], arp->arp_sha[4], arp->arp_sha[5]);

    /* Send the ARP request */
    ssize_t n = sendto(sockfd2, buffer, 42, 0, (struct sockaddr *)&socket_address, sizeof(socket_address));
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

        printf("\nReply from %d.%d.%d.%d\n", arpreply->arp_spa[0], arpreply->arp_spa[1], arpreply->arp_spa[2], arpreply->arp_spa[3]);
        printf("MAC = %02x:%02x:%02x:%02x:%02x:%02x\n", arpreply->arp_sha[0], arpreply->arp_sha[1], arpreply->arp_sha[2], 
                arpreply->arp_sha[3], arpreply->arp_sha[4], arpreply->arp_sha[5]);            

    break;
}

    exit(EXIT_SUCCESS);
}