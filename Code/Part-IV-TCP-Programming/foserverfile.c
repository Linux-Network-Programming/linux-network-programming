#include "lpheader.h"

int main (int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "\nUsage: %s port", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in saddr = {0};
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int enable = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_FASTOPEN, &enable, sizeof(enable)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(atoi(argv[1]));
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    /*if (inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }*/

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

    ssize_t n;
    char buf[BUFSZ];
    int fd = open("newfile.txt", O_RDONLY);
    if (fd == -1) {
        perror("open");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    while (1) {

        if (lseek(fd, 0, SEEK_SET) == -1) {
            perror("lseek");
            exit(EXIT_FAILURE);
        }
        n = read(fd, buf, sizeof(buf));

        client_len = sizeof(client);
        int sockacc = accept(sockfd, (struct sockaddr *)&client, &client_len);
        if (sockacc == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        write(sockacc, buf, n);

        char client_addr[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &client.sin_addr, client_addr, sizeof(client_addr)) == NULL) {
            perror("inet_ntop");
            exit(EXIT_FAILURE);
        }
        printf("\nNew connection from: %s:%d\n", client_addr, ntohs(client.sin_port));
        
       
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            if (write(sockacc, buf, n) == -1) {
                perror("write");
                close(fd);
                close(sockacc);
                exit(EXIT_FAILURE);
            }
        }

        if (n == -1) {
            perror("read");
            close(fd);
            close(sockacc);
            exit(EXIT_FAILURE);
        }
            
        printf("\nFile sent\n");
       
        close(sockacc);        
    }   

    close(sockfd);
    return 0;
}