#include "icmping.h"
#include <time.h>

int main (int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s dest_addr", argv[0]);
        exit (EXIT_FAILURE);
    }

    struct timeval timeout = {
        .tv_sec = 2,
        .tv_usec = 0
    };

    struct {
        struct iphdr header;
        struct icmphdr icmp_header;
        char payload[10];
    } packet;

    const char *src_addr = "192.168.23.117";
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.payload, "Hello ICMP", sizeof(packet.payload));

    // Construct the ICMP header
    packet.icmp_header.type = ICMP_ECHO;
    packet.icmp_header.code = 0;
    packet.icmp_header.un.echo.id = htons(1);
    packet.icmp_header.checksum = 0;
    packet.icmp_header.checksum = checksum(&packet.icmp_header, sizeof(struct icmphdr) + sizeof(packet.payload));    
  
    // Construct the IP header
    packet.header.version = 4;
    packet.header.ihl = 5;      
    packet.header.tos = 0;
    packet.header.tot_len = htons(sizeof(packet));
    packet.header.id = htons(1);
    packet.header.frag_off = 0;
    packet.header.ttl = 64;
    packet.header.protocol = IPPROTO_ICMP;   
    packet.header.check = 0;
    
    if (inet_pton(AF_INET, src_addr, &packet.header.saddr) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    if (inet_pton(AF_INET, argv[1], &packet.header.daddr) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    packet.header.check = checksum(&packet.header, sizeof(struct iphdr));

    // Prepare the socket
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    int enable = 1;
    if (setsockopt(sock, IPPROTO_IP , IP_HDRINCL, &enable, sizeof(enable)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Structures to calculate the time
    struct timespec start, end;

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = packet.header.daddr;

    char buffer[4096];
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);

    for (int ttl=1; ttl<=30; ttl++) {
        
        packet.header.ttl = ttl;
        packet.icmp_header.un.echo.sequence = htons(ttl);

        /* Recalculate ICMP checksum */
        packet.icmp_header.checksum = 0;
        packet.icmp_header.checksum = checksum(&packet.icmp_header, sizeof(struct icmphdr) + sizeof(packet.payload));
        
        /* Recalculate IP checksum because TTL changed */
        packet.header.check = 0;
        packet.header.check = checksum(&packet.header, sizeof(struct iphdr));          
        
        clock_gettime(CLOCK_MONOTONIC, &start);
     
        int snd = sendto(sock, &packet, sizeof(packet), 0, (struct sockaddr *)&dest, sizeof(dest));
        if (snd == -1) {
            perror("sendto");
            exit(EXIT_FAILURE);
        }

        sender_len = sizeof(sender);
        
        int received = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&sender, &sender_len);
        if (received == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("%2d  *\n", ttl);
                continue;
            }
            perror("recvfrom");
            exit(EXIT_FAILURE);
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        long long seconds = end.tv_sec - start.tv_sec;
        long long nanoseconds = end.tv_nsec - start.tv_nsec;
        double rtt = seconds * 1000.0 + nanoseconds / 1000000.0;

        struct iphdr *ip_header = (struct iphdr *)buffer;
        int ip_header_len = ip_header->ihl * 4;
        struct icmphdr *icmp_header = (struct icmphdr *)(buffer + ip_header_len);

        if (icmp_header->type == ICMP_TIME_EXCEEDED && 
            icmp_header->code == 0)
        {

            struct iphdr *original_ip = (struct iphdr *)((char *)icmp_header + 
                                         sizeof(struct icmphdr));
            int original_ip_header_len = original_ip->ihl * 4;
            struct icmphdr *original_icmp = (struct icmphdr *)((char *)original_ip +
                                             original_ip_header_len);

            if (original_ip->saddr == packet.header.saddr &&
                original_ip->daddr == packet.header.daddr &&
                original_icmp->type == ICMP_ECHO &&
                original_icmp->un.echo.id == packet.icmp_header.un.echo.id &&
                original_icmp->un.echo.sequence == htons(ttl)) 
            {
                printf("%2d  From %s  %f ms\n", ttl, inet_ntoa(sender.sin_addr), rtt);
            }

        } else if (icmp_header->type == ICMP_ECHOREPLY && 
                       icmp_header->code == 0 &&
                       icmp_header->un.echo.id == htons(1) &&
                       icmp_header->un.echo.sequence == htons(ttl) &&
                       ip_header->saddr == packet.header.daddr) {
            printf("%2d  From %s  %f ms\n",ttl, inet_ntoa(sender.sin_addr), rtt);
            printf("Destination reached\n");           
            break;
        }
        
    }
    close(sock);
    return 0;
}