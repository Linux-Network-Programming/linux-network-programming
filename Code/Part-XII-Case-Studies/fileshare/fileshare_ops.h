#ifndef FILESHARE_OPS_H
#define FILESHARE_OPS_H

#include <stddef.h>

int send_all(int fd, const void *buf, size_t len);
int recv_line(int fd, char *buf, size_t size);

int list_files(int client_fd);
int download_file(int client_fd);
int upload_file(int client_fd);

#endif
