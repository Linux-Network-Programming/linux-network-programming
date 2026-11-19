#include "udpfile.h"

int main () {

    int sockfd;
    struct sockaddr_in saddr = {0};

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr *) &saddr, sizeof(saddr)) == -1){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    ssize_t n;
    char buf[BUFSZ];
    struct  udp_datagram *packet;
    struct  udp_datagram *ack_packet;
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    
    while ((n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&client, &client_len)) > 0 ) {
        packet = (struct udp_datagram *)buf;  

        switch (packet->type) {

            case DATA:  /* The client requests the file */
                if (n > offsetof(struct udp_datagram, payload)) {
                        int fd = open(packet->payload, O_RDONLY);
                        if (fd == -1) {
                            perror("open");
                            break;
                        }

                         if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
                            perror("setsockopt");
                            exit(EXIT_FAILURE);
                        }
                        
                        unsigned int seq = 1;     
                        char fbuf[PAYLOADSZ];

                        /* Calculate the maximum packet size: datagram + payload */
                        size_t packet_size = offsetof(struct udp_datagram, payload) + PAYLOADSZ;

                        /* Allocate memory for the datagram */
                        struct udp_datagram *contents = malloc(packet_size);
                        if (contents == NULL) {
                            perror("malloc");
                            exit(EXIT_FAILURE);
                        }

                        /* Second way to allocate memory for the datagram */
                        // char sendbuf[BUFSZ];
                        //struct udp_datagram *contents = (struct udp_datagram *)sendbuf;

                        int transfer_ok = 1;
                        ssize_t rd;
                        while ((rd = read(fd, fbuf, sizeof(fbuf))) > 0) {

                            contents->type = DATA;
                            contents->seq = seq;
                            contents->ack = 0;
                            memcpy(contents->payload, fbuf, rd);

                            /* Calculate the current packet size */
                            ssize_t packet_len = offsetof(struct udp_datagram, payload) + rd;
                          
                            char ackbuf[BUFSZ];
                            int acknowledged = 0;
                            for (int retries = 0; retries < MAX_RETRIES; retries++) {
                                
                                if (sendto(sockfd, contents, packet_len, 0, (struct sockaddr *)&client, client_len) == -1) {
                                    perror("sendto");
                                    break;
                                }
                           
                                n = recvfrom(sockfd, ackbuf, sizeof(ackbuf), 0, (struct sockaddr *)&client, &client_len);
                                if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) 
                                    continue;

                                if (n == -1) {
                                    perror("recvfrom");
                                    break;
                                }

                                ack_packet = (struct udp_datagram *)ackbuf;
                                if (n >= offsetof(struct udp_datagram, payload) &&
                                    ack_packet->type == ACK && ack_packet->ack == seq) {
                                    acknowledged = 1;
                                    break;
                                }
                            }        

                            if (acknowledged)  {
                                seq++;
                            }  else {
                                contents->type = ERROR;
                                contents->seq = seq;
                                contents->ack = 0;
                                sendto(sockfd, contents, offsetof(struct udp_datagram, payload), 0, (struct sockaddr *)&client, client_len);
                                transfer_ok = 0;
                                break;
                            }
                                
                        }
                        if (rd == -1) {
                            perror("read");
                            transfer_ok = 0;
                        }
                        close(fd);
                        if (transfer_ok) {
                            contents->type = FIN; 
                            contents->ack = 0;
                            contents->seq = 0;
                            if (sendto(sockfd, contents, offsetof(struct udp_datagram, payload),
                                0, (struct sockaddr *)&client, client_len) == -1) 
                                perror("sendto");
                        }
                        free(contents);
                }
            break;
        }       
    }
    return 0;
}