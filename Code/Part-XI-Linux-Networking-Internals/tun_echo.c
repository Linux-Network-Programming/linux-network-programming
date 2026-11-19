#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#define BUFFER_SIZE 65536

static uint16_t checksum(const void *data, size_t len)
{
    const uint8_t *p = data;
    uint32_t sum = 0;

    while (len >= 2) {
        sum += ((uint16_t)p[0] << 8) | p[1];
        p += 2;
        len -= 2;
    }

    if (len)
        sum += (uint16_t)p[0] << 8;

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return htons((uint16_t)~sum);
}

int
main(void)
{
    int fd;
    struct ifreq ifr;
    unsigned char buffer[BUFFER_SIZE];

    fd = open("/dev/net/tun", O_RDWR);
    if (fd == -1) {
        perror("open /dev/net/tun");
        return EXIT_FAILURE;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_name, "tun0", IFNAMSIZ - 1);

    if (ioctl(fd, TUNSETIFF, &ifr) == -1) {
        perror("ioctl TUNSETIFF");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("Attached to %s\n", ifr.ifr_name);

    for (;;) {
        ssize_t n = read(fd, buffer, sizeof(buffer));

        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("read");
            break;
        }

        if (n < (ssize_t)sizeof(struct iphdr))
            continue;

        struct iphdr *ip = (struct iphdr *)buffer;

        if (ip->version != 4 || ip->ihl < 5)
            continue;

        size_t ip_len = ip->ihl * 4U;

        if ((size_t)n < ip_len || ntohs(ip->tot_len) < ip_len || ntohs(ip->tot_len) > (size_t)n)
            continue;

        size_t packet_len = ntohs(ip->tot_len);

        char src[INET_ADDRSTRLEN];
        char dst[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &ip->saddr, src, sizeof(src));
        inet_ntop(AF_INET, &ip->daddr, dst, sizeof(dst));

        printf("IPv4 packet: %s -> %s, %zu bytes\n", src, dst, packet_len);

        if (ip->protocol != IPPROTO_ICMP || packet_len < ip_len + sizeof(struct icmphdr))
            continue;

        struct icmphdr *icmp = (struct icmphdr *)(buffer + ip_len);

        if (icmp->type != ICMP_ECHO || icmp->code != 0)
            continue;

        /* Construct an ICMP Echo Reply */
        uint32_t addr = ip->saddr;
        ip->saddr = ip->daddr;
        ip->daddr = addr;

        icmp->type = ICMP_ECHOREPLY;
        icmp->checksum = 0;
        icmp->checksum = checksum(icmp, packet_len - ip_len);

        ip->check = 0;
        ip->check = checksum(ip, ip_len);

        if (write(fd, buffer, packet_len) !=
            (ssize_t)packet_len) {
            perror("write");
            break;
        }

        printf("ICMP Echo Reply sent\n");
        fflush(stdout);
    }

    close(fd);
    return EXIT_FAILURE;
}


