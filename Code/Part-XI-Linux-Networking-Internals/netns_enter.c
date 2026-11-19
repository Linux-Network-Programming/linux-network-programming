#define _GNU_SOURCE
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    char path[64];
    int fd;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s PID\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    snprintf(path, sizeof(path), "/proc/%s/ns/net", argv[1]);

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    if (setns(fd, CLONE_NEWNET) == -1) {
        perror("setns");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);

    execl("/bin/zsh", "zsh", (char *)NULL);
    perror("execl");
    exit(EXIT_FAILURE);
}