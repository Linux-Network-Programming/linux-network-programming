#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#define BUFFER_SIZE 65536
#define ARP_LEN 28

static const unsigned char local_mac[ETH_ALEN] =
    { 0x02, 0x00, 0x00, 0x00, 0x20, 0x02 };

struct arp_packet {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    unsigned char sha[ETH_ALEN];
    struct in_addr spa;
    unsigned char tha[ETH_ALEN];
    struct in_addr tpa;
} __attribute__((packed));

static uint16_t checksum(const void *data, size_t len)
{
    const uint8_t *p = data;
    uint32_t sum = 0;

    while (len >= 2) {
        sum += ((uint16_t)p[0] << 8) | p[1];
        p += 2;
        len -= 2;
    }

    if (len) sum += (uint16_t)p[0] << 8;

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return htons((uint16_t)~sum);
}

static int send_frame(int fd, const void *frame, size_t len)
{
    ssize_t n;

    do {
        n = write(fd, frame, len);
    } while (n == -1 && errno == EINTR);

    if (n == -1) {
        perror("write");
        return -1;
    }

    if ((size_t)n != len) {
        fprintf(stderr, "Short TAP frame write\n");
        return -1;
    }
    return 0;
}

static int handle_arp(int fd, unsigned char *frame, size_t len, struct in_addr local_ip)
{
    struct ethhdr *eth = (struct ethhdr *)frame;
    struct arp_packet *arp;
    unsigned char reply[sizeof(struct ethhdr) + sizeof(struct arp_packet)];

    if (len < sizeof(struct ethhdr) + sizeof(struct arp_packet))
        return 0;

    arp = (struct arp_packet *)(frame + sizeof(struct ethhdr));

    if (ntohs(arp->htype) != ARPHRD_ETHER || ntohs(arp->ptype) != ETH_P_IP ||
        arp->hlen != ETH_ALEN || arp->plen != 4 || ntohs(arp->oper) != ARPOP_REQUEST ||
        arp->tpa.s_addr != local_ip.s_addr)
        return 0;

    struct ethhdr *reth = (struct ethhdr *)reply;
    struct arp_packet *rarp = (struct arp_packet *)(reply + sizeof(*reth));

    memcpy(reth->h_dest, eth->h_source, ETH_ALEN);
    memcpy(reth->h_source, local_mac, ETH_ALEN);
    reth->h_proto = htons(ETH_P_ARP);

    rarp->htype = htons(ARPHRD_ETHER);
    rarp->ptype = htons(ETH_P_IP);
    rarp->hlen = ETH_ALEN;
    rarp->plen = 4;
    rarp->oper = htons(ARPOP_REPLY);

    memcpy(rarp->sha, local_mac, ETH_ALEN);
    rarp->spa = local_ip;
    memcpy(rarp->tha, arp->sha, ETH_ALEN);
    rarp->tpa = arp->spa;

    printf("ARP Request received; sending ARP Reply\n");
    return send_frame(fd, reply, sizeof(reply));
}

static int handle_ipv4(int fd, unsigned char *frame, size_t len,
            struct in_addr local_ip)
{
    struct ethhdr *eth = (struct ethhdr *)frame;
    struct iphdr *ip;
    struct icmphdr *icmp;
    size_t ip_len, packet_len;
    unsigned char mac[ETH_ALEN];
    uint32_t addr;

    if (len < sizeof(struct ethhdr) + sizeof(struct iphdr)) return 0;

    ip = (struct iphdr *)(frame + sizeof(struct ethhdr));
    if (ip->version != 4 || ip->ihl < 5) return 0;

    ip_len = ip->ihl * 4U;
    if (len < sizeof(struct ethhdr) + ip_len) return 0;

    packet_len = ntohs(ip->tot_len);
    if (packet_len < ip_len || packet_len > len - sizeof(struct ethhdr))
        return 0;

    if (checksum(ip, ip_len) != 0) return 0;

    /* Do not process fragmented IPv4 packets. */
    if (ntohs(ip->frag_off) & (IP_MF | IP_OFFMASK))
        return 0;

    if (ip->daddr != local_ip.s_addr || ip->protocol != IPPROTO_ICMP ||
        packet_len < ip_len + sizeof(struct icmphdr))
        return 0;

    icmp = (struct icmphdr *) ((unsigned char *)ip + ip_len);
    if (checksum(icmp, packet_len - ip_len) != 0 ||
        icmp->type != ICMP_ECHO || icmp->code != 0)
        return 0;

    /* Swap the Ethernet source and destination addresses. */
    memcpy(mac, eth->h_source, ETH_ALEN);
    memcpy(eth->h_source, local_mac, ETH_ALEN);
    memcpy(eth->h_dest, mac, ETH_ALEN);

    /* Swap the IPv4 source and destination addresses. */
    addr = ip->saddr;
    ip->saddr = ip->daddr;
    ip->daddr = addr;

    /* Construct the ICMP Echo Reply. */
    icmp->type = ICMP_ECHOREPLY;
    icmp->checksum = 0;
    icmp->checksum = checksum(icmp, packet_len - ip_len);

    /* Recalculate the IPv4 header checksum. */
    ip->check = 0;
    ip->check = checksum(ip, ip_len);

    printf("ICMP Echo Request received; sending Echo Reply\n");
    return send_frame(fd, frame, sizeof(struct ethhdr) + packet_len);
}