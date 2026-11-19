#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define WORKERS 3
#define MAX_CLIENTS 100

struct client {
    int fd;
};

struct client clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;


void *worker_thread(void *args) {

    int sockfd = *(int *)args;
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);

    while(1) {

        client_len = sizeof(client);                    
        
        int sockacc = accept(sockfd, (struct sockaddr *)&client, &client_len);
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

        ssize_t n;
        char buf[BUFSIZ];
        int added = 0;
        
        /* Put the new client socket descriptor in the shared list */
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd == -1) {
                clients[i].fd = sockacc;
                added = 1;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
       

        if (!added) {
            fprintf(stderr, "Maximum number of clients reached\n");
            close(sockacc);
            continue;
        }
        
        printf("\nWorker %lu: new connection from: %s:%d\n", (unsigned long)pthread_self(), client_addr, ntohs(client.sin_port)); 
      
        
        char message[BUFSIZ];
        /* Read from the client and broadcast to all clients */
        while ((n = read(sockacc, buf, sizeof(buf))) >0) {
            printf("Worker %lu received %zd bytes: %.*s",(unsigned long)pthread_self(), n, (int)n, buf);
            int len = snprintf(message, sizeof(message), "[%s:%d]: %.*s", client_addr, ntohs(client.sin_port), (int)n, buf);
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd != -1 && clients[i].fd != sockacc) {
                    ssize_t ns = send(clients[i].fd, message, len, MSG_NOSIGNAL);
                    if (ns == -1)
                        perror("send");
                }
            }
            pthread_mutex_unlock(&clients_mutex);
        }
        if (n == -1)
            perror("read");

        /* Remove the client from the shared list */
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd == sockacc) {
                clients[i].fd = -1;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        
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

    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i].fd = -1;

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

    if (bind(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, 10) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < WORKERS; i++) {
        int newt = pthread_create(&thread, NULL, worker_thread, &sockfd);
        if (newt != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(newt));
            continue;
        }
        pthread_detach(thread);      
        
    }
    while (1)
        pause();
}