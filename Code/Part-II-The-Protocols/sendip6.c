#include <stdio.h>
#include <stdlib.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>

int main (int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s ipv6_dest_addr", argv[0]);
        exit (EXIT_FAILURE);
    }

    struct {
        struct ip6_hdr header;
        char payload[10];
    } packet;

    memset(&packet, 0, sizeof(packet));
    memcpy(packet.payload, "Hello IPv6", sizeof(packet.payload));
    packet.header.ip6_ctlun.ip6_un1.ip6_un1_flow = htonl((6 << 28));
    packet.header.ip6_ctlun.ip6_un1.ip6_un1_hlim = 64;
    packet.header.ip6_ctlun.ip6_un1.ip6_un1_nxt = 253;
    packet.header.ip6_ctlun.ip6_un1.ip6_un1_plen = htons(sizeof(packet.payload));

    if (inet_pton(AF_INET6, "fe80::8a2c:8390:898:3a33", &packet.header.ip6_src) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    int ret = inet_pton(AF_INET6, argv[1], &packet.header.ip6_dst);

    if (ret == 0) {
        fprintf(stderr, "Invalid IPv6 address: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    } else if (ret == -1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    int sock = socket(AF_INET6, SOCK_RAW, 253);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    int enable = 1;
    if (setsockopt(sock, IPPROTO_IPV6  , IPV6_HDRINCL , &enable, sizeof(enable)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    /* Send the datagram */
    struct sockaddr_in6 dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin6_family = AF_INET6;
    dest.sin6_scope_id = if_nametoindex("wlan0");
    dest.sin6_addr = packet.header.ip6_dst;
    int snd = sendto(sock, &packet, sizeof(packet), 0, (struct sockaddr *)&dest, sizeof(dest));
    if (snd == -1) {
        perror("sendto");
        exit(EXIT_FAILURE);
    } else 
        printf("Packet sent to: %s\n", argv[1]);

    close(sock);
    return 0;
}