#include "ipc.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int divide_line(char *line, float *result)
{
    char *p = line;
    int count = 0;
    while (*p != '\0') {
        while (isspace((unsigned char)*p))
            ++p;
        if (*p == '\0')
            break;
        char *end;
        errno = 0;
        float number = strtof(p, &end);
        if (p == end || errno == ERANGE || !isfinite(number) ||
            (*end != '\0' && !isspace((unsigned char)*end))) {
            fprintf(stderr, "child: invalid or out-of-range float\n");
            return STATUS_ERROR;
        }
        if (count == 0) {
            *result = number;
            count = 1;
        } else {
            if (number == 0.0f)
                return STATUS_ZERO;
            *result /= number;
            if (!isfinite(*result)) {
                fprintf(stderr, "child: non-finite division result\n");
                return STATUS_ERROR;
            }
        }
        p = end;
    }
    if (count == 0) {
        fprintf(stderr, "child: empty command\n");
        return STATUS_ERROR;
    }
    return STATUS_OK;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: child OUTPUT_FILE\n");
        return EXIT_ERROR;
    }
    fprintf(stderr, "[child PID=%ld PPID=%ld] Started\n",
            (long)getpid(), (long)getppid());
    FILE *output = fopen(argv[1], "w");
    if (output == NULL) {
        perror("child: fopen output");
        send_status(STATUS_ERROR);
        return EXIT_ERROR;
    }
    char *line = NULL;
    size_t capacity = 0;
    int result = 0, final_status = 0;
    if (send_status(STATUS_READY) == -1) {
        result = EXIT_ERROR;
        goto cleanup;
    }
    for (;;) {
        ssize_t length = getline(&line, &capacity, stdin);
        if (length < 0) {
            if (!feof(stdin)) {
                perror("child: getline");
                final_status = STATUS_ERROR;
                result = EXIT_ERROR;
            }
            break;
        }
        float value = 0.0f;
        int status;
        if (memchr(line, '\0', (size_t)length) != NULL) {
            fprintf(stderr, "child: NUL in command\n");
            status = STATUS_ERROR;
        } else {
            status = divide_line(line, &value);
        }
        if (status != STATUS_OK) {
            final_status = status;
            result = status == STATUS_ZERO ? EXIT_ZERO : EXIT_ERROR;
            break;
        }
        if (fprintf(output, "%.*g\n", FLT_DECIMAL_DIG, (double)value) < 0 ||
            fflush(output) == EOF) {
            perror("child: write output");
            final_status = STATUS_ERROR;
            result = EXIT_ERROR;
            break;
        }
        fprintf(stderr, "[child PID=%ld] Result: %.*g\n",
                (long)getpid(), FLT_DECIMAL_DIG, (double)value);
        if (send_status(STATUS_OK) == -1) {
            result = EXIT_ERROR;
            break;
        }
    }

cleanup:
    free(line);
    if (fclose(output) == EOF) {
        perror("child: fclose output");
        final_status = STATUS_ERROR;
        result = EXIT_ERROR;
    }
    if (final_status != 0 && send_status(final_status) == -1)
        result = EXIT_ERROR;
    if (fclose(stdin) == EOF) {
        perror("child: fclose stdin");
        result = EXIT_ERROR;
    }
    if (close_checked(STDOUT_FILENO) == -1)
        result = EXIT_ERROR;
    return result;
}
