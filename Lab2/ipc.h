#ifndef IPC_H
#define IPC_H

#include <stddef.h>

enum { STATUS_READY = 'R', STATUS_OK = 'K', STATUS_ZERO = 'Z', STATUS_ERROR = 'E' };
enum { EXIT_ERROR = 1, EXIT_ZERO = 2 };

int write_all(int fd, const void *data, size_t size);
int read_status(int fd);
int send_status(int status);
int close_checked(int fd);

#endif
