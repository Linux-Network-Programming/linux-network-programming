#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

ssize_t write_all(int fd, const void *buf, size_t count) {
	
	size_t total = 0;
	
	while (total < count) {
  	   ssize_t n = write(fd, (const char *)buf + total,count - total);
	   if (n == -1)
	      return -1;

	   total += n;	
	}
	return total;
}

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
        client_len = sizeof(client);
        int sockacc = accept(sockfd, (struct sockaddr *)&client, &client_len);
        if (sockacc == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        char client_addr[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &client.sin_addr, client_addr, sizeof(client_addr)) == NULL) {
            perror("inet_ntop");
            exit(EXIT_FAILURE);
        }
        printf("\nNew connection from: %s:%d\n", client_addr, ntohs(client.sin_port));
        printf("\nSending the file...");

        int fd = open("newfile.txt", O_RDONLY);
        if (fd == -1) {
            perror("open");
            close(sockacc);
            continue;
        }

        ssize_t n;
        char buf[1024];
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            if (write_all(sockacc, buf, n) == -1) {
	            perror("write");
	            exit(EXIT_FAILURE);
            }
        }

        close(fd);
        close(sockacc);        
    }   

    close(sockfd);
    return 0;
}