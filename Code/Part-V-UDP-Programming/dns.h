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