#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#define BUFSZ 4096

int main(int argc, char *argv[])
{
    res_state state;
    unsigned char answer[BUFSZ];
    int len;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s domain\n", argv[0]);
        return 1;
    }

    /* Allocate and initialize the resolver state */
    state = malloc(sizeof(*state));
    if (state == NULL) {
        perror("malloc");
        return 1;
    }

    if (res_ninit(state) == -1) {
        fprintf(stderr, "res_ninit failed\n");
        free(state);
        return 1;
    }

    /* Query for an IPv4 A record */
    len = res_nquery(state, argv[1], ns_c_in, ns_t_a, answer, sizeof(answer));
    if (len == -1) {
        fprintf(stderr, "res_nquery failed\n");
        res_nclose(state);
        free(state);
        return 1;
    }

    printf("DNS response received: %d bytes\n", len);

    /* Parse the DNS response */
    ns_msg message;
    if (ns_initparse(answer, len, &message) == -1) {
        fprintf(stderr, "ns_initparse failed\n");
        res_nclose(state);
        free(state);
        return 1;
    }

    //printf("Answer records: %d\n", ns_msg_count(message, ns_s_an));

    int count;
    count = ns_msg_count(message, ns_s_an);

    for (int i = 0; i < count; i++) {

        ns_rr record;
        if (ns_parserr(&message, ns_s_an, i, &record) == -1) {
            fprintf(stderr, "ns_parserr failed\n");
            break;
        }

        printf("Name: %s\n", ns_rr_name(record));   
        char ipaddr[INET_ADDRSTRLEN];
        if (ns_rr_type(record) == ns_t_a) {
            inet_ntop(AF_INET, ns_rr_rdata(record), ipaddr, sizeof(ipaddr));
            printf("IPv4 address: %s\n", ipaddr);
        }
        printf("Type: %u\n", ns_rr_type(record));
        printf("Class: %u\n", ns_rr_class(record));
        printf("TTL: %u\n", ns_rr_ttl(record));
        printf("Data length: %u\n", ns_rr_rdlen(record));
        printf("\n");
    }

    /* Close the resolver state */
    res_nclose(state);
    free(state);
    return 0;
}