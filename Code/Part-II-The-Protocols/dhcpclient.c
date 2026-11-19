#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/random.h>
#include <string.h>

#define BUFSZ 1024

#define BOOTREQUEST 1
#define BOOTREPLY 2

#define DHCPDISCOVER 1
#define DHCPOFFER    2
#define DHCPREQUEST  3
#define DHCPDECLINE  4
#define DHCPACK      5
#define DHCPNAK      6
#define DHCPRELEASE  7
#define DHCPINFORM   8

#define DHCP_BROADCAST_FLAG 0x8000

struct dhcp_message {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint8_t  options[];
};

int main () {

    int sockfd;
    struct sockaddr_in server = {0};
    struct sockaddr_in client = {0};

    /* Create the client socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    client.sin_family = AF_INET;    
    client.sin_port = htons(68);
    client.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sockfd, (struct sockaddr *)&client, sizeof(client)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }  

    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    /* Construct the DHCP message */
    
    uint8_t buffer[BUFSZ];
    memset(buffer, 0, sizeof(buffer));

    struct dhcp_message *message = (struct dhcp_message *)buffer;

    message->op = BOOTREQUEST; 
    message->htype = 1;       /* Ethernet */
    message->hlen = 6;        /* Ethernet address length */
    message->hops = 0;

    if (getrandom(&message->xid, sizeof(message->xid), 0) != sizeof(message->xid)) {
        perror("getrandom");
        exit(EXIT_FAILURE);
    }
    
    message->flags = htons(DHCP_BROADCAST_FLAG);

    message->chaddr[0] = 0xfe;
    message->chaddr[1] = 0x37;
    message->chaddr[2] = 0x7a;
    message->chaddr[3] = 0xfb;
    message->chaddr[4] = 0x50;
    message->chaddr[5] = 0x84;

    uint32_t cookie = htonl(0x63825363);
    memcpy(message->options, &cookie, sizeof(cookie));

    message->options[4] = 53;           /* DHCP Message Type */
    message->options[5] = 1;            /* Length */
    message->options[6] = DHCPDISCOVER;    
    message->options[7]  = 55;          /* Parameter Request List */
    message->options[8]  = 4;           /* Length */
    message->options[9]  = 1;           /* Subnet Mask */
    message->options[10] = 3;           /* Router */
    message->options[11] = 6;           /* DNS Server */
    message->options[12] = 15;          /* Domain Name */
    message->options[13] = 255;         /* End */
  
    /* Set the server socket parameters */
    server.sin_family = AF_INET;
    server.sin_port = htons(67);
    server.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    /* Calculate the message length */
    size_t message_len = sizeof(struct dhcp_message) + 14;

    /* Send the DHCP Discover message */
    printf("\nSending DHCP Discover...\n");
    if (sendto(sockfd, message, message_len, 0, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }

    /* Receive the reply */

    uint8_t reply[BUFSZ];
    ssize_t n;
    struct sockaddr_in server_addr = {0};
    socklen_t server_addr_len = sizeof(server_addr);

    n = recvfrom(sockfd, reply, sizeof(reply), 0, (struct sockaddr *)&server_addr, &server_addr_len);   
    if (n == -1) {
        perror("recvfrom");
        exit(EXIT_FAILURE);
    }

    /* Process the DHCP reply */

    /* Validate the reply length */
    if ((size_t)n < sizeof(struct dhcp_message)) {
        printf("DHCP message is too short\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    struct dhcp_message *dhcp_reply = (struct dhcp_message *)reply;
    if (dhcp_reply->op != BOOTREPLY) {
        printf("DHCP message is not DHCP Reply");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (dhcp_reply->xid != message->xid) {
        printf("Transaction ID is not valid");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (memcmp(dhcp_reply->chaddr, message->chaddr, message->hlen) != 0) {
        printf("Client address is not valid\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }  
    
    char newip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &dhcp_reply->yiaddr, newip, sizeof(newip)) == NULL) {
        perror("inet_ntop");
        exit(EXIT_FAILURE);
    }
    printf("\nIP Address: %s", newip);

    /* Parse options to find the parameter list */
    uint8_t *options = dhcp_reply->options + 4;  /* Skip magic cookie */
    size_t options_len = n - sizeof(struct dhcp_message) - 4;

    for (size_t i = 0; i < options_len; ) {
        uint8_t code = options[i];

        if (code == 255)       /* End */
            break;

        if (code == 0) {       /* Padding */
            i++;
            continue;
        }

        if (i + 1 >= options_len)
            break;

        uint8_t len = options[i + 1];
        if (i + 2 + len > options_len) {
            printf("Invalid DHCP option\n");
            break;
        }

        switch (code) {

        case 1:     /* Subnet Mask */
            if (len == 4) {
                char mask[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &options[i + 2], mask, sizeof(mask)) != NULL)
                    printf("\nSubnet Mask: %s", mask);
            }
            break;

        case 3:     /* Router */
            for (size_t j = 0; j + 4 <= len; j += 4) {
                char router[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &options[i + 2 + j], router, sizeof(router)) != NULL)
                    printf("\nRouter: %s", router);
            }
            break;

        case 6:     /* DNS Servers */
            for (size_t j = 0; j + 4 <= len; j += 4) {
                char dns[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &options[i + 2 + j], dns, sizeof(dns)) != NULL)
                    printf("\nDNS Server: %s", dns);
            }
            break;

        case 51:    /* Lease Time */
            if (len == 4) {
                uint32_t lease_time;
                memcpy(&lease_time, &options[i + 2], sizeof(lease_time));
                lease_time = ntohl(lease_time);
                printf("\nLease Time: %u seconds", lease_time);
            }
            break;

        case 53:    /* Message Type */
        if (len != 1) {
            printf("Invalid DHCP Message Type option\n");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        if (options[i + 2] != DHCPOFFER) {
            printf("Message type is not DHCPOFFER\n");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
            break;    

        case 54:    /* DHCP Server Identifier */
            if (len == 4) {
                char server[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &options[i + 2], server, sizeof(server)) != NULL)
                    printf("\nDHCP Server: %s", server);
            }
            break;
        }
        i += 2 + len;
    }

    /* Construct the DHCPREQUEST */

    struct dhcp_message dhcp_request = {0};

    dhcp_request.op = BOOTREQUEST;
    dhcp_request.htype = 1;    /* Ethernet */
    dhcp_request.hlen = 6;
    dhcp_request.xid = message->xid;
    dhcp_request.flags = htons(DHCP_BROADCAST_FLAG);

    memcpy(dhcp_request.chaddr, message->chaddr, message->hlen));

    /* Magic cookie */
    dhcp_request.options[0] = 0x63;
    dhcp_request.options[1] = 0x82;
    dhcp_request.options[2] = 0x53;
    dhcp_request.options[3] = 0x63;

    /* Option 53 - DHCP Message Type */
    dhcp_request.options[4] = 53;
    dhcp_request.options[5] = 1;
    dhcp_request.options[6] = DHCPREQUEST;

    /* Option 50 - Requested IP Address */
    dhcp_request.options[7] = 50;
    dhcp_request.options[8] = 4;
    memcpy(&dhcp_request.options[9], &dhcp_reply->yiaddr, 4);

    /* Option 54 - DHCP Server Identifier */
    dhcp_request.options[13] = 54;
    dhcp_request.options[14] = 4;
    memcpy(&dhcp_request.options[15], &server_addr.sin_addr, 4);

    /* End option */
    dhcp_request.options[19] = 255;

    /* Send the DHCPREQUEST */
    server.sin_addr.s_addr = INADDR_BROADCAST;
    message_len = sizeof(struct dhcp_message) + 20;

    printf("\nSending DHCP Request...\n");
    if (sendto(sockfd, &dhcp_request, message_len, 0, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }

    /* Receive the DHCPACK */

    uint8_t dhcpack[BUFSZ];
    n = recvfrom(sockfd, dhcpack, sizeof(dhcpack), 0, (struct sockaddr *)&server_addr, &server_addr_len);   
    if (n == -1) {
        perror("recvfrom");
        exit(EXIT_FAILURE);
    }

    /* Process the DHCPACK */

    /* Validate the reply length */
    if ((size_t)n < sizeof(struct dhcp_message) + 4) {
        printf("DHCP message is too short\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    struct dhcp_message *dhcp_ack = (struct dhcp_message *)dhcpack;

    if (dhcp_ack->op != BOOTREPLY) {
        printf("DHCP message is not DHCP Reply\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (dhcp_ack->xid != message->xid) {
        printf("Transaction ID is not valid\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (memcmp(dhcp_ack->chaddr, message->chaddr, message->hlen) != 0) {
        printf("Client address is not valid\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }  

    options = dhcp_ack->options + 4;
    options_len = (size_t)n - sizeof(struct dhcp_message) - 4;

    for (size_t i = 0; i < options_len; ) {
        uint8_t code = options[i];

        if (code == 255)       /* End */
            break;

        if (code == 0) {       /* Padding */
            i++;
            continue;
        }

        if (i + 1 >= options_len)
            break;

        uint8_t len = options[i + 1];
        if (i + 2 + len > options_len) {
            printf("Invalid DHCP option\n");
            break;
        }

        switch (code) {

        case 51:    /* Lease Time */
        if (len == 4) {
            uint32_t lease_time;
            memcpy(&lease_time, &options[i + 2], sizeof(lease_time));
            lease_time = ntohl(lease_time);
            printf("\nLease Time: %u seconds", lease_time);
        }
        break;

        case 53:    /* Message Type */
            if (len != 1) {
                printf("Invalid DHCP Message Type option\n");
                close(sockfd);
                exit(EXIT_FAILURE);
            }
            if (options[i + 2] != DHCPACK) {
                printf("Message type is not DHCPACK\n");
                close(sockfd);
                exit(EXIT_FAILURE);
            }
            break;    

        case 54:    /* DHCP Server Identifier */
            if (len == 4) {
                char server[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &options[i + 2], server, sizeof(server)) != NULL)
                    printf("\nDHCP Server: %s", server);
            }
            break;
        }
        i += 2 + len;
    }
  

    close(sockfd);
    return 0;
}