#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

static int waitChild(pid_t pid) {
    int status;
    pid_t result;
    do { result = waitpid(pid, &status, 0); } while (result < 0 && errno == EINTR);
    if (result < 0) { perror("waitpid"); return 1; }
    return !WIFEXITED(status) || WEXITSTATUS(status) != 0;
}

/* Use close + dup as requested in part 1 of the lab. */
static void redirectPipe(int fd, int target) {
    if (fd == target) return;
    if (close(target) < 0 && errno != EBADF) { perror("close"); _exit(1); }
    if (dup(fd) != target) { perror("dup"); _exit(1); }
    close(fd);
}

int main(void) {
    int pipefd[2];
    if (pipe(pipefd) < 0) { perror("pipe"); return 1; }
    fprintf(stderr, "(parent_process>forking…)\n");
    pid_t child1 = fork();
    if (child1 < 0) {
        perror("fork"); close(pipefd[0]); close(pipefd[1]); return 1;
    }
    if (child1 == 0) {
        fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe…)\n");
        close(pipefd[0]);
        redirectPipe(pipefd[1], STDOUT_FILENO);
        fprintf(stderr, "(child1>going to execute cmd: ls -l…)\n");
        char *cmd[] = {"ls", "-l", NULL};
        execvp(cmd[0], cmd);
        perror("ls");
        _exit(127);
    }
    fprintf(stderr, "(parent_process>created process with id: %ld)\n", (long)child1);
    fprintf(stderr, "(parent_process>closing the write end of the pipe…)\n");
    close(pipefd[1]);
    fprintf(stderr, "(parent_process>forking…)\n");
    pid_t child2 = fork();
    if (child2 < 0) {
        perror("fork");
        close(pipefd[0]);
        kill(child1, SIGKILL);
        waitChild(child1);
        return 1;
    }
    if (child2 == 0) {
        fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe…)\n");
        redirectPipe(pipefd[0], STDIN_FILENO);
        fprintf(stderr, "(child2>going to execute cmd: tail -n 2…)\n");
        char *cmd[] = {"tail", "-n", "2", NULL};
        execvp(cmd[0], cmd);
        perror("tail");
        _exit(127);
    }
    fprintf(stderr, "(parent_process>created process with id: %ld)\n", (long)child2);
    fprintf(stderr, "(parent_process>closing the read end of the pipe…)\n");
    close(pipefd[0]);
    fprintf(stderr, "(parent_process>waiting for child processes to terminate…)\n");
    int failed = waitChild(child1);
    failed |= waitChild(child2);
    fprintf(stderr, "(parent_process>exiting…)\n");
    return failed;
}
