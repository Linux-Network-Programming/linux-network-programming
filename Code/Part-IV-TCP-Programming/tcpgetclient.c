#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "lpheader.h"

#define PORT 2468
#define BUFSZ 1024
#define MAX_FILENAME 256
#define LIST 0
#define DOWNLOAD 1
#define UPLOAD 2

struct clientmsg {
    int command; 
    int filename_len;
    char filename[MAX_FILENAME];
};

void display_progress(int file_size, int received) {
    static int previous = -1;

    if (file_size == 0)
        return;

    int percentage = received * 100 / file_size;

    if (percentage != previous) {
        fprintf(stderr, "\rDOWNLOADED: %d%%", percentage);
        fflush(stderr);
        previous = percentage;
    }
}

void request_list(int sock) {

    ssize_t n;
    char buf[BUFSZ];

    struct clientmsg msg = {0};
    msg.command = LIST;

    if(write_all(sock, &msg, sizeof(msg)) == -1){ 
        perror("write");
        return;
    }

    while ((n = read(sock, buf, sizeof(buf))) > 0) {
        if (write_all(STDOUT_FILENO, buf, n) == -1) {
            perror("write");
            break;
        }
    }

    if (n == -1)
        perror("read");

}

void download_file(int sock, char filename[]) {
    
    int fd;
    ssize_t n;
    char buf[BUFSZ];

    struct clientmsg msg = {0};
    msg.command = DOWNLOAD;
    msg.filename_len = strlen(filename);
    memcpy(msg.filename, filename, msg.filename_len);

    if (write_all(sock, &msg, sizeof(msg)) == -1) {
        perror("write");
        return;
    }

    if ((fd = open("saved.txt", O_CREAT | O_TRUNC | O_WRONLY, 0644)) == -1) {
        perror("open");
        return;
    }

    uint64_t file_size;
    if (read_all(sock, &file_size, sizeof(file_size)) != sizeof(file_size)) {
        fprintf(stderr, "Failed to receive file size\n");
        close(fd);
        return;
    }

    uint64_t received = 0;
    while ((n = read(sock, buf, sizeof(buf))) > 0) {
        if (write_all(fd, buf, n) == -1) {
            perror("write");
            break;
        }
        received += n;
        display_progress(file_size, received);
    }

    close(fd);
}

void upload_file(int sock, char filename[]) {

    int fd;
    ssize_t n;
    char buf[BUFSZ];

    struct clientmsg msg = {0};
    msg.command = UPLOAD;
    msg.filename_len = strlen(filename);
    memcpy(msg.filename, filename, msg.filename_len);

    if (write_all(sock, &msg, sizeof(msg)) == -1) {
        perror("write");
        return;
    }

    if ((fd = open(filename, O_RDONLY)) == -1) {
        perror("open");
        return;
    }

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        if (write_all(sock, buf, n) == -1) {
            perror("write");
            break;
        }
    }
    if (n == -1)
        perror("read");

    close(fd);
    shutdown(sock, SHUT_WR);
}

int main (int argc, char *argv[]) {

    if (argc != 4) {
        fprintf(stderr, "\nUsage: %s ip_server [0|1|2] filename", argv[0]);
        fprintf(stderr, "\n0 LIST\n1 DOWNLOAD\n2 UPLOAD\n", argv[0]);
        fprintf(stderr, "\nExample: %s 10.10.10.10 0 0", argv[0]);
        fprintf(stderr, "\nExample: %s 10.10.10.10 1 file.txt", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in daddr = {0};
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    daddr.sin_family = AF_INET;
    daddr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, argv[1], &daddr.sin_addr) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    
    if (connect(sockfd, (struct sockaddr *) &daddr, sizeof(struct sockaddr_in)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    int command = atoi(argv[2]);
    switch (command) {
    case LIST:
        request_list(sockfd);
        break;
    case DOWNLOAD:
        download_file(sockfd, argv[3]);
        break;
    case UPLOAD:
        upload_file(sockfd, argv[3]);
        break;    
    default:
        break;
    }

    close(sockfd);
    return 0;
}