#include "fileshare_ops.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 12345
#define BACKLOG     10

/* Display the available actions */
static int send_menu(int client_fd)
{
    const char menu[] =
        "\n"
        "===== FILE SHARING SERVER =====\n"
        "1. List files\n"
        "2. Download a file\n"
        "3. Upload a file\n"
        "0. Exit\n"
        "Select an option: ";

    return send_all(client_fd, menu, sizeof(menu) - 1);
}

/* Handle a client connection */
static void *worker_thread(void *arg)
{
    int client_fd = *(int *)arg;
    char choice[32];

    free(arg);

    printf("Client connected (fd=%d)\n", client_fd);

    while (1) {
        if (send_menu(client_fd) < 0)
            break;

        if (recv_line(client_fd, choice, sizeof(choice)) < 0)
            break;

        if (strcmp(choice, "1") == 0) {
            if (list_files(client_fd) < 0)
                break;
        }
        else if (strcmp(choice, "2") == 0) {
            if (download_file(client_fd) < 0)
                break;
        }
        else if (strcmp(choice, "3") == 0) {
            if (upload_file(client_fd) < 0)
                break;
        }
        else if (strcmp(choice, "0") == 0) {
            break;
        }
        else {
            const char msg[] = "Invalid selection.\n";

            if (send_all(client_fd, msg, sizeof(msg) - 1) < 0)
                break;
        }
    }

    printf("Client disconnected (fd=%d)\n", client_fd);
    close(client_fd);
    return NULL;
}

/* Main function */
int main(void)
{
    int server_fd;
    struct sockaddr_in server_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("File sharing server listening on port %d\n", SERVER_PORT);

    while (1) {
        int *client_fd;
        pthread_t tid;
        int fd;

        fd = accept(server_fd, NULL, NULL);
        if (fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        client_fd = malloc(sizeof(*client_fd));
        if (client_fd == NULL) {
            perror("malloc");
            close(fd);
            continue;
        }

        *client_fd = fd;
        int ret = pthread_create(&tid, NULL, worker_thread, client_fd);
        if (ret != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(ret));
            free(client_fd);
            close(fd);
            continue;
        }
        pthread_detach(tid);
    }
    close(server_fd);
    return 0;
}
