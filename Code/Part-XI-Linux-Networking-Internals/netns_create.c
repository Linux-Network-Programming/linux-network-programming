#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    if (unshare(CLONE_NEWNET) == -1) {
        perror("unshare");
        exit(EXIT_FAILURE);
    }

    execl("/bin/zsh", "zsh", (char *)NULL);
    perror("execl");
    exit(EXIT_FAILURE);
}