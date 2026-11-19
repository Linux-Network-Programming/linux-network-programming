#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>

#define BUFSZ 1024
#define PORT 12346
#define MCAST "239.220.220.220"

void *worker_thread(void *args) {

    int sockfd = *(int *)args;
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    char message[BUFSZ];
    ssize_t n;

    while(1) {   
        n = recvfrom(sockfd, message, sizeof(message), 0, (struct sockaddr *)&client, &client_len);
        if (n == -1) {
            perror("recvfrom");
            break;
        }

        char client_addr[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &client.sin_addr, client_addr, sizeof(client_addr)) == NULL) {
            perror("inet_ntop");
            exit(EXIT_FAILURE);
        }

        printf("\nFrom: %s:%d: %.*s\n", client_addr, ntohs(client.sin_port), (int)n, message);
        write(STDOUT_FILENO, "> ", 2);
        fflush(stdout);
    }        
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

    struct ip_mreqn mreq = {0};
    if (inet_pton(AF_INET, MCAST, &mreq.imr_multiaddr) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    mreq.imr_address.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1) {
            perror("IP_ADD_MEMBERSHIP");
            exit(EXIT_FAILURE);
    }

    int loop = 0;
    setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

    int newt = pthread_create(&thread, NULL, worker_thread, &sockfd);
    if (newt != 0) 
        fprintf(stderr, "pthread_create: %s\n", strerror(newt));
    pthread_detach(thread);      

    ssize_t n;
    char buf[BUFSZ];
    socklen_t daddr_len = sizeof(daddr);

    daddr.sin_family = AF_INET;
    daddr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, MCAST, &daddr.sin_addr) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    write(STDOUT_FILENO, "> ", 2);
    fflush(stdout);
    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0)  {

       if (buf[0] == 'Q') {
           if (setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) == -1) {
                perror("IP_DROP_MEMBERSHIP");
            }
            break;
        }

        if ((sendto(sockfd, buf, n, 0, (struct sockaddr *)&daddr, daddr_len)) == -1)
            perror("sendto");
        write(STDOUT_FILENO, "> ", 2);
        fflush(stdout);
    }
}