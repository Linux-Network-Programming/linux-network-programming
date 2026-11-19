#include "server.h"

int main(void)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(sockfd);
        return EXIT_FAILURE;
    }
    printf("UDP server listening on port %d\n", PORT);

    for (;;) {
        char request[BUFSIZE];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        ssize_t n = recvfrom(sockfd, request, sizeof(request), 0, (struct sockaddr *)&client_addr,
                             &client_len);

        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("recvfrom");
            continue;
        }

        /* Retrieve the current date and time */
        time_t now = time(NULL);
        struct tm tm;
        if (now == (time_t)-1 || localtime_r(&now, &tm) == NULL) {
            fprintf(stderr, "Unable to retrieve the current time\n");
            continue;
        }

        char date_msg[64];
        char time_msg[64];

        if (strftime(date_msg, sizeof(date_msg), "Date: %Y-%m-%d", &tm) == 0 ||
            strftime(time_msg, sizeof(time_msg), "Time: %H:%M:%S", &tm) == 0) {
            fprintf(stderr, "Unable to format date and time\n");
            continue;
        }

        /* Prepare the two messages */
        struct iovec iov[2] = {
            {
                .iov_base = date_msg,
                .iov_len = strlen(date_msg)
            },
            {
                .iov_base = time_msg,
                .iov_len = strlen(time_msg)
            }
        };

        struct mmsghdr msgs[2] = {0};
        for (int i = 0; i < 2; i++) {
            msgs[i].msg_hdr.msg_name = &client_addr;
            msgs[i].msg_hdr.msg_namelen = client_len;
            msgs[i].msg_hdr.msg_iov = &iov[i];
            msgs[i].msg_hdr.msg_iovlen = 1;
        }

        /* Send both datagrams in one system call */
        int sent = sendmmsg(sockfd, msgs, 2, 0);
        if (sent == -1) {
            perror("sendmmsg");
            continue;
        }

        if (sent != 2) {
            fprintf(stderr, "sendmmsg: sent %d of 2 messages\n", sent);
            continue;
        }

        printf("Sent %u bytes and %u bytes to the client\n", msgs[0].msg_len, msgs[1].msg_len);
    }
    close(sockfd);
    return EXIT_SUCCESS;
}