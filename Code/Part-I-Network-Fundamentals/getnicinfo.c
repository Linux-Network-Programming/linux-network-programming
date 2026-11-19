#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <unistd.h>

int main (int argc, char *argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Usage: %s nic", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct ifreq ifr;            

    /* Create the socket to retrieve information */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);  
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, argv[1], IFNAMSIZ -1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    /* Retrieve the local MAC */
    unsigned char *mac;                
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == -1) {
        perror("SIOCGIFHWADDR") ;
        exit(EXIT_FAILURE);
    }
    mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
    printf("Local MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    /* Retrieve the NIC index */
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) == -1) {
        perror("SIOCGIFINDEX");
        exit(EXIT_FAILURE);
    }
    printf("Interface index: %d\n", ifr.ifr_ifindex);

    /* Retrieve the local IPv4 address */
    char ip[INET_ADDRSTRLEN];

    if (ioctl(sockfd, SIOCGIFADDR, &ifr) == -1) {
        perror("SIOCGIFADDR");
        exit(EXIT_FAILURE);
    }

    if (inet_ntop(AF_INET, &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr,
              ip, sizeof(ip)) == NULL) {
        perror("inet_ntop");
        exit(EXIT_FAILURE);
    }
    printf("Local IP: %s\n", ip);
    close(sockfd);
    exit(EXIT_SUCCESS);
}