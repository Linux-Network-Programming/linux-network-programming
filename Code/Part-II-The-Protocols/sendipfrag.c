#include "iphead.h"

int main (int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "Usage: %s dest_addr payload_size", argv[0]);
        exit (EXIT_FAILURE);
    }

    int payload_size = atoi(argv[2]);
    int num_fragments = payload_size / 1480;
    int last_pay_size = payload_size % 1480;
    if (last_pay_size != 0)
        num_fragments += 1;

    char *original_payload = malloc(payload_size);
    if (original_payload == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memset(original_payload, 64, payload_size);

    struct {
        struct iphdr header;
        char payload[1480];
    } fragment;

    memset(&fragment, 0, sizeof(fragment));
    int frag_size;
    struct sockaddr_in dest;
    int enable = 1;

    int sock = socket(AF_INET, SOCK_RAW, 253);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &enable, sizeof(enable)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < num_fragments; i++) {
        memset(&fragment.header, 0, sizeof(fragment.header));
        fragment.header.version = 4;
        fragment.header.ihl = 5;
        fragment.header.id = htons(1);
        
        int offset = (i * 1480) / 8;
        fragment.header.frag_off = htons(offset); 
    
        if (i == num_fragments - 1)
            frag_size = (last_pay_size == 0) ? 1480 : last_pay_size;
        else
            frag_size = 1480;
        
        if (i < num_fragments - 1)
            fragment.header.frag_off |= htons(IP_MF);

        memcpy(fragment.payload, original_payload + offset * 8, frag_size);
        fragment.header.tot_len = htons(sizeof(struct iphdr) + frag_size);
        fragment.header.ttl = 64;
        fragment.header.protocol = 253;
        fragment.header.check = 0;

        if (inet_pton(AF_INET, "192.168.1.145", &fragment.header.saddr) != 1) {
            perror("inet_pton");
            exit(EXIT_FAILURE);
        }
        if (inet_pton(AF_INET, argv[1], &fragment.header.daddr) != 1) {
            perror("inet_pton");
            exit(EXIT_FAILURE);
        }
        fragment.header.check = ip_checksum(&fragment.header, sizeof(struct iphdr));

        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_addr.s_addr = fragment.header.daddr;
        int packet_len = sizeof(struct iphdr) + frag_size;
        if (sendto(sock, &fragment, packet_len, 0, (struct sockaddr *)&dest, sizeof(dest)) == -1) {
            perror("sendto");
            exit(EXIT_FAILURE);
        }
        printf("Fragment %d/%d (%d bytes) sent to %s\n", i + 1, num_fragments, frag_size, argv[1]);    
    }

    free(original_payload);
    close(sock);
    return 0;
}