#include "ipc.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int write_all(int fd, const void *data, size_t size)
{
    const char *p = data;
    while (size > 0) {
        ssize_t n = write(fd, p, size);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            if (n == 0)
                errno = EIO;
            return -1;
        }
        p += n;
        size -= (size_t)n;
    }
    return 0;
}

int read_status(int fd)
{
    unsigned char status;
    ssize_t n;
    do {
        n = read(fd, &status, 1);
    } while (n < 0 && errno == EINTR);
    if (n == 1)
        return status;
    if (n == 0)
        errno = EPIPE;
    return -1;
}

int send_status(int status)
{
    unsigned char byte = (unsigned char)status;
    if (write_all(STDOUT_FILENO, &byte, 1) == -1) {
        perror("child: send status");
        return -1;
    }
    return 0;
}

int close_checked(int fd)
{
    if (close(fd) == -1) {
        perror("close");
        return -1;
    }
    return 0;
}
