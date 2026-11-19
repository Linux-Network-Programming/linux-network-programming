#include "udpfile.h"

int main(int argc, char *argv[]) {
    
    if (argc != 2) {
        fprintf(stderr, "Usage: %s filename", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "10.186.166.129" , &server.sin_addr) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    socklen_t server_len = sizeof(server);
   
    char buf[BUFSZ];
    struct udp_datagram *packet = (struct udp_datagram *)buf;  

    packet->type = DATA;
    packet->seq = 0;
    packet->ack = 0;
    size_t filename_len = strlen(argv[1]) + 1;
    memcpy(packet->payload, argv[1], filename_len);
    
    ssize_t packet_len = offsetof(struct udp_datagram, payload) + filename_len; 
    if (sendto(sockfd, packet, packet_len, 0, (struct sockaddr *)&server, server_len) == -1) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }

    int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    ssize_t n, w;
    unsigned int expected_seq = 1;
    while((n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&server, &server_len)) >0) {
        packet = (struct udp_datagram *)buf;  

        switch (packet->type) {
        
        case DATA:

            size_t header_len = offsetof(struct udp_datagram, payload);
            if ((size_t)n < header_len)
                continue;

            if (packet->seq == expected_seq) {
                size_t payload_len = n - header_len;
                w = write(fd, packet->payload, payload_len);
        
                if (w == -1) {
                    perror("write");
                    exit(EXIT_FAILURE);
                }
                if ((size_t)w < payload_len) {
                    fprintf(stderr, "Partial write: %zd instead of %zu", w, payload_len);
                    exit(EXIT_FAILURE);
                }
                expected_seq++;
            }

            struct udp_datagram ack = {
                    .type = ACK,
                    .seq = 0,
                    .ack = packet->seq
            };
            if (sendto(sockfd, &ack, header_len, 0, (struct sockaddr *)&server, server_len) == -1) {
                perror("sendto");
                exit(EXIT_FAILURE);
            }

        break;
        
        case FIN:
            printf("Transfer complete\n");
            close(sockfd);
            close(fd);
            return 0; 

        case ERROR:
            printf("Transfer error: packet: %u", packet->seq);
            close(sockfd);
            close(fd);
            return EXIT_FAILURE;
        
        }
    }

    close(fd);
    close(sockfd);
    return 0;
}