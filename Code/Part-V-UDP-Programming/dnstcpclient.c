#include "dns.h"

int main(int argc, char *argv[]) {
    
    if (argc != 2) {
        fprintf(stderr, "Usage: %s domain", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    if (inet_pton(AF_INET, DNS_SERVER, &server.sin_addr) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
   
    char buf[BUFSZ];
    size_t offset = 0;

    /* Prepare the header */
    struct dns_header header;
    header.id = htons(1);
    header.qdcount = htons(1);
    header.ancount = htons(0);
    header.nscount = htons(0);
    header.arcount = htons(0);
    header.flags = htons(DNS_RD);

    memcpy(buf + offset, &header, sizeof(header));
    offset += sizeof(header);

    uint16_t qtype = htons(1);
    uint16_t qclass = htons(1);

    /* Encode QNAME */
    size_t qnamelen = encode_qname(buf + offset, argv[1]);
    offset += qnamelen;

    /* Add QTYPE and QCLASS */
    memcpy(buf + offset, &qtype, sizeof(qtype));
    offset += sizeof(qtype);
    memcpy(buf + offset, &qclass, sizeof(qclass));
    offset += sizeof(qclass);

    socklen_t server_len = sizeof(server);
    if (connect(sockfd, (struct sockaddr *)&server, server_len) == -1){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    /* Calculate the DNS message length */
    uint16_t msglength = htons(offset);

    /* Send the length prefix */
    ssize_t s = send(sockfd, &msglength, sizeof(msglength), 0);
    if (s == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    if (s != sizeof(msglength)) { 
        fprintf(stderr, "Incomplete length prefix sent\n"); 
        exit(EXIT_FAILURE); 
    }

    /* Send the DNS Query */
    printf("Sending DNS query for %s\n", argv[1]);
    ssize_t n = send(sockfd, buf, offset, 0);
    if (n == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    if ((size_t)n != offset) {
        fprintf(stderr, "Incomplete DNS query sent\n");
        exit(EXIT_FAILURE);
    }

    /* Retrieve the response length */ 
    uint16_t response_msglength; 
    n = recv(sockfd, &response_msglength, sizeof(response_msglength), MSG_WAITALL); 
    if (n == -1) { 
        perror("recv"); 
        exit(EXIT_FAILURE); 
    } 

    if (n != sizeof(response_msglength)) { 
        fprintf(stderr, "Incomplete response length received\n"); 
        exit(EXIT_FAILURE); 
    } 
    response_msglength = ntohs(response_msglength);

    /* Retrieve the DNS response */ 
    char rbuf[BUFSZ]; 
    if (response_msglength > sizeof(rbuf)) { 
        fprintf(stderr, "DNS response is too large\n"); 
        exit(EXIT_FAILURE); 
    } 
    
    n = recv(sockfd, rbuf, response_msglength, MSG_WAITALL); 
    if (n == -1) { 
        perror("recv"); 
        exit(EXIT_FAILURE); 
    } 
    
    if ((size_t)n != response_msglength) { 
        fprintf(stderr, "Incomplete DNS response received\n"); 
        exit(EXIT_FAILURE); 
    } 
    printf("\nResponse size: %zd bytes\n", n);
    
    /* Parse the DNS header */
    struct dns_header server_header;
  
    memcpy(&server_header, rbuf, sizeof(server_header));
    server_header.id      = ntohs(server_header.id);
    server_header.flags   = ntohs(server_header.flags);
    server_header.qdcount = ntohs(server_header.qdcount);
    server_header.ancount = ntohs(server_header.ancount);
    server_header.nscount = ntohs(server_header.nscount);
    server_header.arcount = ntohs(server_header.arcount);

    printf("Transaction ID: %u\n", server_header.id);
    printf("Answers: %u\n", server_header.ancount);

    /* Parse the Question section */

    size_t soffset = sizeof(struct dns_header);

    char qname[256];
    size_t namelen = decode_qname(qname, rbuf + soffset, rbuf);
    soffset += namelen;
    printf("Name: %s\n", qname);

    uint16_t type;
    memcpy(&type, rbuf + soffset, sizeof(type));
    type = ntohs(type);
    soffset += sizeof(type);
    printf("Type: %u\n", type);

    uint16_t class;
    memcpy(&class, rbuf + soffset, sizeof(class));
    class = ntohs(class);
    soffset += sizeof(class);
    printf("Class: %u\n", class);

    /* Parse the Resource Record (Answer section) */
    for (int i = 0; i < server_header.ancount; i++) {

        /* Parse Resource Record */
        char rrname[256];
        namelen = decode_qname(rrname, rbuf + soffset, rbuf);
        soffset += namelen;

        printf("\nResource Record %d", i + 1);
        printf("\nName: %s\n", rrname);

        uint16_t rrtype;
        memcpy(&rrtype, rbuf + soffset, sizeof(rrtype));
        rrtype = ntohs(rrtype);
        soffset += sizeof(rrtype);
        printf("Type: %u\n", rrtype);

        uint16_t rrclass;
        memcpy(&rrclass, rbuf + soffset, sizeof(rrclass));
        rrclass = ntohs(rrclass);
        soffset += sizeof(rrclass);
        printf("Class: %u\n", rrclass);

        uint32_t ttl;
        memcpy(&ttl, rbuf + soffset, sizeof(ttl));
        ttl = ntohl(ttl);
        soffset += sizeof(ttl);
        printf("TTL: %u\n", ttl);

        uint16_t rdlength;
        memcpy(&rdlength, rbuf + soffset, sizeof(rdlength));
        rdlength = ntohs(rdlength);
        soffset += sizeof(rdlength);
        printf("RDLength: %u\n", rdlength);

        if (rrtype == 5) {
            char cname[256];
            decode_qname(cname, rbuf + soffset, rbuf);
            printf("CNAME: %s\n", cname);
        } else if (rrtype == 1 && rdlength == 4) {
            struct in_addr addr;
            memcpy(&addr, rbuf + soffset, sizeof(addr));
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr, ip, sizeof(ip));
            printf("Address: %s\n", ip);
        }

        /* Move to the next Resource Record */
        soffset += rdlength;
    }

    return 0;
}