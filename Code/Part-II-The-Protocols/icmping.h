#include <stdio.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <linux/icmp.h>
#include <sys/time.h>
#include <errno.h>

uint16_t checksum(const void *data, size_t len)
{
    const uint16_t *buf = data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }

    /* Handle an odd number of bytes (should not occur for IPv4 headers). */
    if (len == 1)
        sum += *(const uint8_t *)buf;

    /* Fold carries into the lower 16 bits. */
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (uint16_t)~sum;
}