#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 65536

int main(void)
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
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, "tap0", IFNAMSIZ - 1);

    if (ioctl(fd, TUNSETIFF, &ifr) == -1) {
        perror("ioctl TUNSETIFF");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Attached to %s\n", ifr.ifr_name);

    for (;;) {
        ssize_t n = read(fd, buffer, sizeof(buffer));

        if (n == -1) {
            perror("read");
            break;
        }

        if (n < (ssize_t)sizeof(struct ethhdr))
            continue;

        struct ethhdr *eth = (struct ethhdr *)buffer;

        printf("Ethernet frame: ");
        printf("%02x:%02x:%02x:%02x:%02x:%02x -> ",
               eth->h_source[0], eth->h_source[1],
               eth->h_source[2], eth->h_source[3],
               eth->h_source[4], eth->h_source[5]);

        printf("%02x:%02x:%02x:%02x:%02x:%02x ",
               eth->h_dest[0], eth->h_dest[1],
               eth->h_dest[2], eth->h_dest[3],
               eth->h_dest[4], eth->h_dest[5]);

        printf("EtherType: 0x%04x, Length: %zd bytes\n",
               ntohs(eth->h_proto), n);

        fflush(stdout);
    }

    close(fd);
    return EXIT_FAILURE;
}

