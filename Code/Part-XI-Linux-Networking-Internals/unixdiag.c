#include "unixdiag.h"

int main(void)
{
    int fd;
    struct sockaddr_nl local = {0};
    struct sockaddr_nl kernel = {0};

    struct {
        struct nlmsghdr nlh;
        struct unix_diag_req req;
    } request = {0};

    char buffer[BUF_SIZE];
    struct sockaddr_nl peer;
    struct iovec iov;
    struct msghdr msg;
    unsigned int seq = 1;

    fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_SOCK_DIAG);
    if (fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    local.nl_family = AF_NETLINK;
    local.nl_pid = getpid();
    if (bind(fd, (struct sockaddr *) &local, sizeof(local)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    kernel.nl_family = AF_NETLINK;

    request.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(request.req));
    request.nlh.nlmsg_type = SOCK_DIAG_BY_FAMILY;
    request.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    request.nlh.nlmsg_seq = seq;
    request.req.sdiag_family = AF_UNIX;
    request.req.udiag_states = (1U << TCP_LISTEN) | (1U << TCP_ESTABLISHED);

    request.req.udiag_show = UDIAG_SHOW_NAME | UDIAG_SHOW_PEER | UDIAG_SHOW_RQLEN;
    if (sendto(fd, &request, request.nlh.nlmsg_len, 0, (struct sockaddr *) &kernel,
               sizeof(kernel)) == -1) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }

    printf("%-10s %-10s %-12s %-30s %-10s %-10s\n",
           "Inode", "Type", "State", "Name", "Peer", "Queues");

    for (;;) {
        memset(&peer, 0, sizeof(peer));
        memset(&msg, 0, sizeof(msg));

        iov.iov_base = buffer;
        iov.iov_len = sizeof(buffer);

        msg.msg_name = &peer;
        msg.msg_namelen = sizeof(peer);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        ssize_t n = recvmsg(fd, &msg, 0);
        if (n == -1) {
            if (errno == EINTR) continue;
            perror("recvmsg");
            break;
        }

        if (peer.nl_pid != 0) {
            fprintf(stderr, "Unexpected Netlink sender\n");
            break;
        }

        for (struct nlmsghdr *nlh = (struct nlmsghdr *) buffer;
             NLMSG_OK(nlh, n);
             nlh = NLMSG_NEXT(nlh, n)) {

            if (nlh->nlmsg_seq != seq) continue;
            if (nlh->nlmsg_type == NLMSG_DONE) goto done;
            if (nlh->nlmsg_type == NLMSG_ERROR) {
                struct nlmsgerr *err = (struct nlmsgerr *) NLMSG_DATA(nlh);
                if (err->error != 0) {
                    errno = -err->error;
                    perror("Netlink");
                    goto done;
                }
                continue;
            }
            if (nlh->nlmsg_type == SOCK_DIAG_BY_FAMILY)
                process_message(nlh);
        }
    }
done:
    close(fd);
    return EXIT_SUCCESS;
}
