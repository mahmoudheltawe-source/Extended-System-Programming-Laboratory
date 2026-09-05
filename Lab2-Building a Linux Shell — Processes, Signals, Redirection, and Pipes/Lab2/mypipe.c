#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

int main(void) {
    int ends[2], status;
    char buffer[128];
    if (pipe(ends) == -1) { perror("pipe"); return EXIT_FAILURE; }
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork"); close(ends[0]); close(ends[1]); return EXIT_FAILURE;
    }
    if (pid == 0) {
        const char message[] = "hello\n";
        size_t sent = 0;
        close(ends[0]);
        while (sent < sizeof(message) - 1) {
            ssize_t n = write(ends[1], message + sent, sizeof(message) - 1 - sent);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) { perror("write"); _exit(EXIT_FAILURE); }
            sent += (size_t)n;
        }
        close(ends[1]);
        _exit(EXIT_SUCCESS);
    }
    close(ends[1]);
    int result = EXIT_SUCCESS;
    for (;;) {
        ssize_t n = read(ends[0], buffer, sizeof(buffer));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { perror("read"); result = EXIT_FAILURE; break; }
        if (n == 0) break;
        if (fwrite(buffer, 1, (size_t)n, stdout) != (size_t)n) {
            perror("stdout"); result = EXIT_FAILURE; break;
        }
    }
    close(ends[0]);
    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR) continue;
        perror("waitpid"); return EXIT_FAILURE;
    }
    if (fflush(stdout) == EOF) { perror("stdout"); result = EXIT_FAILURE; }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? result : EXIT_FAILURE;
}
