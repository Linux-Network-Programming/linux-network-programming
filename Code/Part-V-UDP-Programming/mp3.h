#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdint.h>
#include <time.h>

#define BUFSZ 2048
#define PORT 12346

struct worker_args {
    int sock;
    struct sockaddr_in client;
    char song[];
};
struct mp3_frame {
    int bitrate;
    int sample_rate;
    int padding;
    int frame_size;
};

static const int bitrate_table[] = {
    0, 32, 40, 48, 56, 64, 80, 96,
    112, 128, 160, 192, 224, 256, 320
};

static const int samplerate_table[] = {
    44100, 48000, 32000
};

int parse_mp3_header(unsigned char header[4], struct mp3_frame *frame);
int read_mp3_frame(int fd, unsigned char *buffer, size_t buffer_size, struct mp3_frame *frame);
int find_first_mp3_frame(int fd);