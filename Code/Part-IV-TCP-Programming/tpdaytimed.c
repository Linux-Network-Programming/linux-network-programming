#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define WORKERS 3

struct thread_args {
    int sockfd;
};

void *worker_thread(void *args) {

    struct thread_args *t = args;
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);

    while(1) {

        client_len = sizeof(client);                    

        int sockacc = accept(t->sockfd, (struct sockaddr *)&client, &client_len);
        if (sockacc == -1) {
            perror("accept");
            continue;
        }

        char client_addr[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &client.sin_addr, client_addr, sizeof(client_addr)) == NULL) {
            perror("inet_ntop");
            close(sockacc);
            continue;
        }
        printf("\nWorker %lu: new connection from: %s:%d\n", (unsigned long)pthread_self(), client_addr, ntohs(client.sin_port));                

        time_t tm = time(NULL);
        char *date = ctime(&tm);
        write(sockacc, date, strlen(date));
        close(sockacc);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
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

    saddr.sin_addr.s_addr = htonl(INADDR_ANY);

    /*if (inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }*/

    if (bind(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, 10) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < WORKERS; i++) {
        struct thread_args *t = malloc(sizeof(*t));
         if (t == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        t->sockfd = sockfd;

        int newt = pthread_create(&thread, NULL, worker_thread, t);
        if (newt != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(newt));
            free(t);
            continue;
        }
        pthread_detach(thread);      
        
    }
    while (1)
        pause();
}