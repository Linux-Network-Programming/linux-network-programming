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
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
    };

    const char request[] = "request";
    if (sendto(sockfd, request, sizeof(request) - 1, 0, (struct sockaddr *)&server_addr,
               sizeof(server_addr)) == -1) {
        perror("sendto");
        close(sockfd);
        return EXIT_FAILURE;
    }

    char date_buf[BUFSIZE];
    char time_buf[BUFSIZE];
    struct iovec iov[2] = {
        {
            .iov_base = date_buf,
            .iov_len = sizeof(date_buf)
        },
        {
            .iov_base = time_buf,
            .iov_len = sizeof(time_buf)
        }
    };

    struct mmsghdr msgs[2] = {0};
    for (int i = 0; i < 2; i++) {
        msgs[i].msg_hdr.msg_iov = &iov[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
    }

    int received = recvmmsg(sockfd, msgs, 2, 0, NULL);
    if (received == -1) {
        perror("recvmmsg");
        close(sockfd);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < received; i++) {
        printf("%.*s\n", (int)msgs[i].msg_len,
               (char *)iov[i].iov_base);
    }

    close(sockfd);
    return EXIT_SUCCESS;
}