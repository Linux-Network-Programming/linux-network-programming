#include "fileshare_ops.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define SHARED_DIR "shared"
#define STATUS_OK  0
#define STATUS_ERR 1

int send_all(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0)
            return -1;
        p += n;
        len -= n;
    }
    return 0;
}

static int recv_all(int fd, void *buf, size_t len)
{
    char *p = buf;
    while (len > 0) {
        ssize_t n = recv(fd, p, len, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0)
            return -1;
        p += n;
        len -= n;
    }
    return 0;
}

int recv_line(int fd, char *buf, size_t size)
{
    size_t i = 0;
    char c;
    while (i < size - 1) {
        if (recv_all(fd, &c, 1) < 0)
            return -1;
        if (c == '\n') {
            buf[i] = '\0';
            return 0;
        }
        if (c == '\0')
            return -1;
        buf[i++] = c;
    }
    return -1;
}

static int valid_filename(const char *name)
{
    return name[0] != '\0' &&
           strcmp(name, ".") != 0 &&
           strcmp(name, "..") != 0 &&
           strchr(name, '/') == NULL;
}

static int send_status(int fd, uint8_t status)
{
    return send_all(fd, &status, sizeof(status));
}

int list_files(int client_fd)
{
    DIR *dir;
    struct dirent *entry;
    struct stat st;

    dir = opendir(SHARED_DIR);
    if (dir == NULL)
        return -1;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        if (fstatat(dirfd(dir), entry->d_name, &st, AT_SYMLINK_NOFOLLOW) < 0)
            continue;

        if (!S_ISREG(st.st_mode))
            continue;

        if (send_all(client_fd, entry->d_name, strlen(entry->d_name)) < 0 ||
            send_all(client_fd, "\n", 1) < 0) {
            closedir(dir);
            return -1;
        }
    }
    closedir(dir);
    return send_all(client_fd, ".\n", 2);
}

/*
 * Send a file to the client
 *
 * Protocol:
 *   Client -> filename\n
 *   Server -> status + file size + file contents
 */
int download_file(int client_fd)
{
    char filename[NAME_MAX + 1];
    int dirfd_shared, file_fd;
    struct stat st;
    uint8_t status;
    uint64_t file_size;

    if (recv_line(client_fd, filename, sizeof(filename)) < 0)
        return -1;

    if (!valid_filename(filename))
        return send_status(client_fd, STATUS_ERR);

    dirfd_shared = open(SHARED_DIR, O_RDONLY | O_DIRECTORY);
    if (dirfd_shared < 0)
        return send_status(client_fd, STATUS_ERR);

    file_fd = openat(dirfd_shared, filename, O_RDONLY | O_NOFOLLOW);
    close(dirfd_shared);
    if (file_fd < 0)
        return send_status(client_fd, STATUS_ERR);

    if (fstat(file_fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        close(file_fd);
        return send_status(client_fd, STATUS_ERR);
    }

    status = STATUS_OK;
    file_size = htobe64((uint64_t)st.st_size);

    if (send_status(client_fd, status) < 0 ||
        send_all(client_fd, &file_size, sizeof(file_size)) < 0) {
        close(file_fd);
        return -1;
    }

    char buffer[4096];
    ssize_t n;
    while ((n = read(file_fd, buffer, sizeof(buffer)) * Main function.
 */) > 0) {
        if (send_all(client_fd, buffer, n) < 0) {
            close(file_fd);
            return -1;
        }
    }
    close(file_fd);
    return n < 0 ? -1 : 0;
}

/*
 * Receive a file from the client
 *
 * Protocol:
 *   Client -> filename\n + file size + file contents
 *   Server -> status
 */
int upload_file(int client_fd)
{
    char filename[NAME_MAX + 1];
    int dirfd_shared, file_fd;
    uint64_t file_size;
    uint8_t status;
    char buffer[4096];

    if (recv_line(client_fd, filename, sizeof(filename)) < 0)
        return -1;

    if (!valid_filename(filename))
        return send_status(client_fd, STATUS_ERR);

    if (recv_all(client_fd, &file_size, sizeof(file_size)) < 0)
        return -1;

    file_size = be64toh(file_size);

    dirfd_shared = open(SHARED_DIR, O_RDONLY | O_DIRECTORY);
    if (dirfd_shared < 0)
        return send_status(client_fd, STATUS_ERR);

    file_fd = openat(dirfd_shared, filename, O_WRONLY | O_CREAT | O_TRUNC |
                     O_NOFOLLOW, 0644);
    close(dirfd_shared);

    if (file_fd < 0)
        return send_status(client_fd, STATUS_ERR);

    while (file_size > 0) {
        size_t chunk = file_size < sizeof(buffer) ? (size_t)file_size : sizeof(buffer);
        if (recv_all(client_fd, buffer, chunk) < 0) {
            close(file_fd);
            return -1;
        }

        size_t written = 0;
        while (written < chunk) {
            ssize_t n = write(file_fd, buffer + written, chunk - written);
            if (n < 0) {
                if (errno == EINTR) continue;
                close(file_fd);
                return -1;
            }

            if (n == 0) {
                close(file_fd);
                return -1;
            }
            written += n;
        }
        file_size -= chunk;
    }
    close(file_fd);
    status = STATUS_OK;
    return send_status(client_fd, status);
}
