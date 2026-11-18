#include "ipc.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    char *filename = NULL, *line = NULL;
    size_t filename_capacity = 0, line_capacity = 0;
    int pipe1[2] = {-1, -1}, pipe2[2] = {-1, -1};
    int result = EXIT_ERROR;
    pid_t child_pid = -1;
    struct sigaction action = {0};
    action.sa_handler = SIG_IGN;
    if (sigemptyset(&action.sa_mask) == -1 ||
        sigaction(SIGPIPE, &action, NULL) == -1) {
        perror("parent: sigaction");
        return EXIT_ERROR;
    }

    fprintf(stderr, "[parent PID=%ld] Output file: ", (long)getpid());
    ssize_t length = getline(&filename, &filename_capacity, stdin);
    if (length < 0) {
        if (feof(stdin))
            fprintf(stderr, "parent: output filename is required\n");
        else
            perror("parent: getline filename");
        goto cleanup;
    }
    if (memchr(filename, '\0', (size_t)length) != NULL) {
        fprintf(stderr, "parent: NUL in filename\n");
        goto cleanup;
    }
    if (length > 0 && filename[length - 1] == '\n')
        filename[--length] = '\0';
    if (length == 0) {
        fprintf(stderr, "parent: empty filename\n");
        goto cleanup;
    }
    if (pipe(pipe1) == -1) {
        perror("parent: pipe1");
        goto cleanup;
    }
    if (pipe(pipe2) == -1) {
        perror("parent: pipe2");
        goto cleanup;
    }
    child_pid = fork();
    if (child_pid == -1) {
        perror("parent: fork");
        goto cleanup;
    }
    if (child_pid == 0) {
        if (dup2(pipe1[0], STDIN_FILENO) == -1 ||
            dup2(pipe2[1], STDOUT_FILENO) == -1) {
            perror("child: dup2");
            _exit(EXIT_ERROR);
        }
        if (close_checked(pipe1[0]) == -1 || close_checked(pipe1[1]) == -1 ||
            close_checked(pipe2[0]) == -1 || close_checked(pipe2[1]) == -1)
            _exit(EXIT_ERROR);
        execl("./child", "child", filename, (char *)NULL);
        perror("child: execl ./child");
        _exit(EXIT_ERROR);
    }

    int closed1 = close_checked(pipe1[0]);
    pipe1[0] = -1;
    int closed2 = close_checked(pipe2[1]);
    pipe2[1] = -1;
    if (closed1 == -1 || closed2 == -1)
        goto cleanup;
    fprintf(stderr, "[parent PID=%ld] child PID=%ld\n",
            (long)getpid(), (long)child_pid);
    int status = read_status(pipe2[0]);
    if (status != STATUS_READY) {
        if (status == -1)
            perror("parent: child readiness");
        else
            fprintf(stderr, "parent: child could not open output file\n");
        goto cleanup;
    }

    result = 0;
    for (;;) {
        fprintf(stderr, "[parent PID=%ld] Numbers (Ctrl+D to finish): ",
                (long)getpid());
        length = getline(&line, &line_capacity, stdin);
        if (length < 0) {
            if (!feof(stdin)) {
                perror("parent: getline numbers");
                result = EXIT_ERROR;
            }
            break;
        }
        if (write_all(pipe1[1], line, (size_t)length) == -1 ||
            (line[length - 1] != '\n' && write_all(pipe1[1], "\n", 1) == -1)) {
            perror("parent: write pipe1");
            result = EXIT_ERROR;
            break;
        }
        status = read_status(pipe2[0]);
        if (status == STATUS_OK) {
            fprintf(stderr, "[parent PID=%ld] Result saved\n", (long)getpid());
            continue;
        }
        if (status == STATUS_ZERO) {
            fprintf(stderr, "[parent PID=%ld] Division by zero; stopping\n",
                    (long)getpid());
            result = EXIT_ZERO;
        } else {
            if (status == -1)
                perror("parent: read pipe2");
            else
                fprintf(stderr, "parent: child reported an error\n");
            result = EXIT_ERROR;
        }
        break;
    }

cleanup:
    free(filename);
    free(line);
    for (int i = 0; i < 2; ++i) {
        if (pipe1[i] != -1 && close_checked(pipe1[i]) == -1)
            result = EXIT_ERROR;
        if (pipe2[i] != -1 && close_checked(pipe2[i]) == -1)
            result = EXIT_ERROR;
    }
    if (child_pid > 0) {
        int status;
        pid_t waited;
        do {
            waited = waitpid(child_pid, &status, 0);
        } while (waited == -1 && errno == EINTR);
        if (waited == -1) {
            perror("parent: waitpid");
            result = EXIT_ERROR;
        } else if (WIFEXITED(status)) {
            fprintf(stderr, "[parent PID=%ld] Child exit code: %d\n",
                    (long)getpid(), WEXITSTATUS(status));
            if (result == 0)
                result = WEXITSTATUS(status);
        } else {
            fprintf(stderr, "parent: child terminated by signal %d\n",
                    WTERMSIG(status));
            result = EXIT_ERROR;
        }
    }
    return result;
}
