#include "tap_echo.h"

int main(void)
{
    int fd;
    struct ifreq ifr;
    unsigned char buffer[BUFFER_SIZE];
    struct in_addr local_ip;

    if (inet_pton(AF_INET, "192.168.20.2", &local_ip) != 1) {
        fprintf(stderr, "Invalid local IPv4 address\n");
        return EXIT_FAILURE;
    }

    fd = open("/dev/net/tun", O_RDWR);
    if (fd == -1) {
        perror("open /dev/net/tun");
        return EXIT_FAILURE;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, "tap0", IFNAMSIZ - 1);

    if (ioctl(fd, TUNSETIFF, &ifr) == -1) {
        perror("ioctl TUNSETIFF");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Attached to %s; endpoint 192.168.20.2\n", ifr.ifr_name);
    fflush(stdout);

    for (;;) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("read");
            break;
        }

        if ((size_t)n < sizeof(struct ethhdr))
            continue;

        struct ethhdr *eth = (struct ethhdr *)buffer;

        switch (ntohs(eth->h_proto)) {
        case ETH_P_ARP:
            if (handle_arp(fd, buffer, (size_t)n, local_ip) == -1)
                goto out;
            break;

        case ETH_P_IP:
            if (handle_ipv4(fd, buffer, (size_t)n, local_ip) == -1)
                goto out;
            break;

        default:
            break;
        }
        fflush(stdout);
    }

out:
    close(fd);
    return EXIT_FAILURE;
}