#include <dirent.h>
#include <signal.h>
#include <wait.h>
#include <sys/stat.h>
#include "lpheader.h"

#define MAX_FILENAME 256
#define LIST 0
#define DOWNLOAD 1
#define UPLOAD 2

struct clientmsg {
    int command; 
    int filename_len;
    char filename[MAX_FILENAME];
};

void send_list(int socket) {
    struct dirent **namelist;
    int n;

 	n = scandir("./", &namelist, NULL, alphasort);
 	if (n == -1) {
		perror("scandir");
 		exit(EXIT_FAILURE);
 	}
	   for (int i = 0; i < n; i++) {
        size_t len = strlen(namelist[i]->d_name);

           if (write_all(socket, namelist[i]->d_name, len) == -1 ||
               write_all(socket, "\n", 1) == -1) {
            perror("write");
            free(namelist[i]);
            break;
        }
        free(namelist[i]);
    }
    free(namelist);

}

void send_file(int socket, char filename[]) {
    int fd;
    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return;
    }
    ssize_t n;
    char buf[BUFSIZ];
    struct stat st;

    /* Retrieve the file size */
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        close(fd);
        return;
    }

    /* Send the file size */
    uint64_t file_size = st.st_size;
    if (write_all(socket, &file_size, sizeof(file_size)) == -1) {
        perror("write");
        return;
    }

    /* Read and send the file contents */
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        if (write_all(socket, buf, n) == -1){
            perror("write");
            close(fd);
            break;
        }
    }
    if (n == -1)
        perror("read");
    close(fd);
}

void receive_file(int socket, char filename[]) {
    ssize_t n;
    char buf[BUFSIZ];

    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1){
        perror("open");
        return;
    }

    while ((n = read(socket, buf, sizeof(buf))) > 0) {
        if (write_all(fd, buf, n) == -1){
            perror("write");
            break;
        }
    }
    if (n == -1)
        perror("read");
    close(fd);
}


void sigchld_handler(int sig)
{
	while (waitpid(-1, NULL, WNOHANG) > 0)
	;
}

int main (int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "\nUsage: %s port", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in saddr = {0};

    struct sigaction sa = {0};
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    	perror("sigaction");
    	exit(EXIT_FAILURE);
    }

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

        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            close(sockfd);
            char client_addr[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &client.sin_addr, client_addr, sizeof(client_addr)) == NULL) {
                perror("inet_ntop");
                _exit(EXIT_FAILURE);
            }
            printf("\nNew connection from: %s:%d\n", client_addr, ntohs(client.sin_port));
        
            struct clientmsg msg;         
            if (read_all(sockacc, &msg, sizeof(msg)) != sizeof(msg)) {
                fprintf(stderr, "Incomplete message received\n");
                close(sockacc);
                _exit(EXIT_FAILURE);
            } 
            if (msg.filename_len < 0 || msg.filename_len >= MAX_FILENAME) {
                const char *error = "Invalid filename length";
                write(sockacc, error, strlen(error));
                close(sockacc);
                _exit(EXIT_FAILURE);
            }
            msg.filename[msg.filename_len] = '\0';

            switch (msg.command) {
                case LIST:
                    send_list(sockacc);
                    break;
                case DOWNLOAD:
                    send_file(sockacc, msg.filename);
                    break;
                case UPLOAD:
                    receive_file(sockacc, msg.filename);
                    break;
                default:
                    fprintf(stderr, "Unknown command: %d\n", msg.command);
                    break;
            }
            close(sockacc);
            _exit(EXIT_SUCCESS);
        }     
        close(sockacc);       
           
    } 
    close(sockfd);
    return 0;
}   

    