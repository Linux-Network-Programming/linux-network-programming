#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>

#define DNS_SERVER "192.168.12.1"
#define PORT 53
#define BUFSZ 1024
#define DNS_RD  0x0100  /* RD Recursion Desired */

/* DNS Header is 12 bytes */
struct dns_header {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

size_t encode_qname(char *dst, const char *name)
{
    size_t pos = 0;
    const char *label = name;

    while (*label) {
        const char *dot = strchr(label, '.');
        size_t len = dot ? (size_t)(dot - label) : strlen(label);

        dst[pos++] = len;
        memcpy(dst + pos, label, len);
        pos += len;

        if (!dot) break;

        label = dot + 1;
    }
    dst[pos++] = 0;
    return pos;
}

size_t decode_qname(char *dst, const char *src, const char *msg)
{
    const char *start = src;
    size_t pos = 0;
    size_t dstpos = 0;

    while (1) {

        if ((src[pos] & 0xc0) == 0xc0) {

            uint16_t pointer = ((src[pos] & 0x3f) << 8) |
                               (unsigned char)src[pos + 1];

            src = msg + pointer;
            pos = 0;

            continue;
        }

        if (src[pos] == 0) {
            pos++;
            break;
        }

        unsigned int len = (unsigned char)src[pos++];

        if (dstpos != 0)
            dst[dstpos++] = '.';

        memcpy(dst + dstpos, src + pos, len);
        dstpos += len;
        pos += len;
    }

    dst[dstpos] = '\0';

    if ((start[0] & 0xc0) == 0xc0)
        return 2;

    return pos;
}

int main(int argc, char *argv[]) {
    
    if (argc != 2) {
        fprintf(stderr, "Usage: %s domain", argv[0]);
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

    printf("Sending DNS query for %s\n", argv[1]);
    ssize_t n = sendto(sockfd, buf, offset, 0, (struct sockaddr *)&server, sizeof(server));
    if (n == -1) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }

    if ((size_t)n != offset) {
        fprintf(stderr, "Incomplete DNS query sent\n");
        exit(EXIT_FAILURE);
    }

    /* Retrieve the response */
    char rbuf[BUFSZ];
    socklen_t server_len = sizeof(server);

    ssize_t r = recvfrom(sockfd, rbuf, sizeof(rbuf), 0, (struct sockaddr *)&server, &server_len);
    if (r == -1) {
        perror("recvfrom");
        exit(EXIT_FAILURE);
    }
    printf("\nResponse size: %zd bytes\n", r);

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