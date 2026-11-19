#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

struct thread_args {
    struct sockaddr_in client;
    socklen_t client_len; 
    int sockacc;
};

void *worker_thread(void *args) {

    struct thread_args *t = args;

    char client_addr[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &t->client.sin_addr, client_addr, sizeof(client_addr)) == NULL) {
        perror("inet_ntop");
        close(t->sockacc);
        free(t);
        exit(EXIT_FAILURE);
    }
    printf("\nNew connection from: %s:%d\n", client_addr, ntohs(t->client.sin_port));

    time_t tm = time(NULL);
    char *date = ctime(&tm);
    write(t->sockacc, date, strlen(date));
    close(t->sockacc);
    free(t);
    return NULL;
}

int main (int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "\nUsage: %s port", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in saddr = {0};
    pthread_t thread;
 

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(atoi(argv[1]));
    if (inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if (bind(sockfd, (struct sockaddr *) &saddr, sizeof(saddr)) == -1){
        perror("bind");
        exit(EXIT_FAILURE);
    }
    
    if (listen(sockfd, 10) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    
    while (1) {
        struct thread_args *t = malloc(sizeof(*t));
         if (t == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        t->client_len = sizeof(t->client);        
        t->sockacc = accept(sockfd, (struct sockaddr *)&t->client, &t->client_len);
        if (t->sockacc == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        
        int newt = pthread_create(&thread, NULL, worker_thread, t);
        if (newt != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(newt));
            close(t->sockacc);
            free(t);
            continue;
        }
        pthread_detach(thread);
    }   

    close(sockfd);
    return 0;
}