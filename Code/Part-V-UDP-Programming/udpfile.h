#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stddef.h>
#include <sys/time.h>
#include <errno.h>

#define PORT 12346
#define BUFSZ 1024
#define PAYLOADSZ 8192
#define MAX_RETRIES 3

#define DATA  1
#define ACK   2
#define FIN   3
#define ERROR 4

struct udp_datagram {
    unsigned int type;
    unsigned int seq;
    unsigned int ack;
    char payload[];
};