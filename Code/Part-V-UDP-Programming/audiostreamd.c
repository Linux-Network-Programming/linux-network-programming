#include "mp3.h"

void *worker_thread(void *arg)
{
    struct worker_args *args = arg;
    int sockfd = args->sock;
    struct sockaddr_in client = args->client;
    char *song = args->song;

    unsigned char audio[BUFSZ];
    struct mp3_frame frame;

    int fd = open(song, O_RDONLY);
    if (fd == -1) {
        perror("open");
        free(args);
        return NULL;
    }

    if (find_first_mp3_frame(fd) == -1) {
        fprintf(stderr, "Unable to locate MP3 audio data\n");
        close(fd);
        free(args);
        return NULL;
    }

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    while (1) {
        int n = read_mp3_frame(fd, audio, sizeof(audio), &frame);
        if (n == 0) break;

        if (n == -1) {
            fprintf(stderr, "Invalid MP3 frame\n");
            break;
        }

        ssize_t s = sendto(sockfd, audio, n, 0, (struct sockaddr *)&client, sizeof(client));
        if (s == -1) {
            perror("sendto");
            break;
        }

        /* MPEG-1 Layer III contains 1152 samples per frame */
        double duration = 1152.0 / frame.sample_rate;
        long nanoseconds = (long)(duration * 1000000000.0);

        next.tv_sec += nanoseconds / 1000000000L;
        next.tv_nsec += nanoseconds % 1000000000L;
        if (next.tv_nsec >= 1000000000L) {
            next.tv_sec++;
            next.tv_nsec -= 1000000000L;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }
    close(fd);
    free(args);
    return NULL;
}

int main() {
    int sockfd;
    struct sockaddr_in saddr = {0};
    struct sockaddr_in daddr = {0};
    pthread_t thread;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(PORT); 
    if (bind(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    while (1) {
        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);
        char buf[BUFSZ];

        int n = recvfrom(sockfd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&client, &client_len);
        if (n == -1) {
            perror("recvfrom");
            continue;
        }
        
        struct worker_args *args;
        args = malloc(sizeof(*args) + n + 1);
        if (args == NULL) {
            perror("malloc");
            continue;
        }

        args->sock = sockfd;
        args->client = client;
        memcpy(args->song, buf, n);
        args->song[n] = '\0';
        int ret = pthread_create(&thread, NULL, worker_thread, args);
        if (ret != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(ret));
            free(args);
            continue;
        }
        pthread_detach(thread);
    }
}