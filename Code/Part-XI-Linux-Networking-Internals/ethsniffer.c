#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 65536

int main(int argc, char *argv[])
{
    int sockfd;
    unsigned int ifindex;
    struct sockaddr_ll sll;
    unsigned char buffer[BUFFER_SIZE];
    ssize_t n;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <interface>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    ifindex = if_nametoindex(argv[1]);
    if (ifindex == 0) {
        perror("if_nametoindex");
        exit(EXIT_FAILURE);
    }

    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&sll, 0, sizeof(sll));
    sll.sll_family   = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex  = ifindex;

    if (bind(sockfd, (struct sockaddr *)&sll, sizeof(sll)) == -1) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Listening on interface %s (index %u)...\n", argv[1], ifindex);

    /* Receive and process Ethernet frames */
    for (;;) {
        n = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
        if (n == -1) {
            perror("recvfrom");
            break;
        }

        /* Check that the frame contains an Ethernet header */
        if (n < sizeof(struct ethhdr))
            continue;

        struct ethhdr *eth = (struct ethhdr *)buffer;
        printf("\nEthernet frame (%zd bytes)\n", n);

        printf("Source MAC:      ""%02x:%02x:%02x:%02x:%02x:%02x\n",
               eth->h_source[0], eth->h_source[1], eth->h_source[2], eth->h_source[3],
               eth->h_source[4], eth->h_source[5]);

        printf("Destination MAC: ""%02x:%02x:%02x:%02x:%02x:%02x\n",
               eth->h_dest[0], eth->h_dest[1], eth->h_dest[2], eth->h_dest[3],
               eth->h_dest[4], eth->h_dest[5]);

        printf("Protocol:        0x%04x\n", ntohs(eth->h_proto));
    }
    close(sockfd);
    return 0;
}