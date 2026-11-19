#include <stdio.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

uint16_t ip_checksum(const void *data, size_t len)
{
    const uint16_t *buf = data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }

    /* Handle an odd number of bytes (should not occur for IPv4 headers). */
    if (len == 1)
        sum += *(const uint8_t *)buf;

    /* Fold carries into the lower 16 bits. */
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (uint16_t)~sum;
}

int main (int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s dest_addr", argv[0]);
        exit (EXIT_FAILURE);
    }

    /* Construct the datagram */
    struct {
        struct iphdr header;
        char payload[10];
    } packet;

    memset(&packet, 0, sizeof(packet));
    memcpy(packet.payload, "Hello IPv4", sizeof(packet.payload));
    packet.header.version = 4;
    packet.header.ihl = 5;      // no options (20 bytes)
    packet.header.tos = 0;
    packet.header.tot_len = htons(sizeof(packet));
    packet.header.id = htons(1);
    packet.header.frag_off = 0;
    packet.header.ttl = 64;
    packet.header.protocol = 253;    // Experimental protocol  (RFC 3692)
    packet.header.check = 0;
    if (inet_pton(AF_INET, "192.168.1.145", &packet.header.saddr) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    if (inet_pton(AF_INET, argv[1], &packet.header.daddr) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    packet.header.check = ip_checksum(&packet.header, sizeof(struct iphdr));

    /* Prepare the socket */
    int sock = socket(AF_INET, SOCK_RAW, 253);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    int enable = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &enable, sizeof(enable)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    /* Send the datagram */
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = packet.header.daddr;
    int snd = sendto(sock, &packet, sizeof(packet), 0, (struct sockaddr *)&dest, sizeof(dest));
    if (snd == -1) {
        perror("sendto");
        exit(EXIT_FAILURE);
    } else 
        printf("Packet sent to: %s\n", argv[1]);

    close(sock);
    return 0;

}