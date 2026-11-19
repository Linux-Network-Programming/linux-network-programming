#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>
#include <linux/tcp.h>

/* FreeBSD */
/*
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
*/

#define BUFSZ 1024

struct message {
    uint32_t length;
    char *data;
};

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

ssize_t read_all(int fd, void *buf, size_t count)
{
    size_t total = 0;

    while (total < count) {
        ssize_t n = read(fd, (char *)buf + total, count - total);

        if (n == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (n == 0)
            break;

        total += n;
     
    }

    return total;
}

ssize_t read_message(int fd, char *buf, size_t size)
{
    size_t total = 0;

    while (total < size - 1) {
        ssize_t n = read(fd, buf + total, 1);

        if (n == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (n == 0)
            break;

        total += n;

        if (buf[total - 1] == '\n')
            break;
    }

    buf[total] = '\0';
    return total;
}