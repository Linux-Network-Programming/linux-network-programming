#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define BUFSZ 1024
#define PORT 1357

int main (int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "\nUsage: %s ip_server", argv[0]);
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


    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        ssize_t n;
        char buf[BUFSZ];
        printf("> ");
        fflush(stdout);
        while ((n = read(sockfd, buf, sizeof(buf))) > 0) {
            if (write(STDOUT_FILENO, buf, n) == -1) {
                perror("write");
                break;
            }
            write(STDOUT_FILENO, "> ", 2);
            fflush(stdout);
        }
        if (n == -1)
            perror("read");

        close(sockfd);
        exit(EXIT_SUCCESS);
    } 

    ssize_t n;
    char buf[BUFSZ];

    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0)  {
        if (write(sockfd, buf, n) == -1)
            perror("write");
        write(STDOUT_FILENO, "> ", 2);
        fflush(stdout);
    }

    shutdown(sockfd, SHUT_WR);
    waitpid(pid, NULL,0);
    close(sockfd);

}