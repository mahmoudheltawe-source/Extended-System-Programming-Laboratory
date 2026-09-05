#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include "LineParser.h"

#define BUFFER_SIZE 2048

/* Collect children even while the shell is waiting for user input. */
static void reap_children(int sig) {
    int saved_errno = errno;
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
    errno = saved_errno;
}

void execute(cmdLine *cmd) {
    if (cmd->inputRedirect && !freopen(cmd->inputRedirect, "r", stdin)) {
        perror("input redirection");
        _exit(EXIT_FAILURE);
    }
    if (cmd->outputRedirect && !freopen(cmd->outputRedirect, "w", stdout)) {
        perror("output redirection");
        _exit(EXIT_FAILURE);
    }
    execvp(cmd->arguments[0], cmd->arguments);
    perror(cmd->arguments[0]);
    _exit(EXIT_FAILURE);
}

static void signal_command(cmdLine *cmd, int sig) {
    char *end;
    long value;
    if (cmd->argCount != 2) {
        fprintf(stderr, "Usage: %s <positive process id>\n", cmd->arguments[0]);
        return;
    }
    errno = 0;
    value = strtol(cmd->arguments[1], &end, 10);
    if (errno || *end || end == cmd->arguments[1] || value <= 0 || value > INT_MAX) {
        fprintf(stderr, "Invalid process id: %s\n", cmd->arguments[1]);
        return;
    }
    if (kill((pid_t)value, sig) == -1)
        perror(cmd->arguments[0]);
    else
        printf("Sent %s to process %ld\n", sig == SIGCONT ? "SIGCONT" : "SIGKILL", value);
}

int main(int argc, char **argv) {
    char cwd[PATH_MAX], input[BUFFER_SIZE];
    int debug = 0;
    struct sigaction action = {0};
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-d") == 0) debug = 1;
        else { fprintf(stderr, "Usage: %s [-d]\n", argv[0]); return EXIT_FAILURE; }
    }
    action.sa_handler = reap_children;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &action, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }
    for (;;) {
        if (getcwd(cwd, sizeof(cwd))) printf("%s> ", cwd);
        else { perror("getcwd"); printf("> "); }
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin)) {
            if (ferror(stdin)) { perror("input"); return EXIT_FAILURE; }
            break;
        }
        if (!strchr(input, '\n') && !feof(stdin)) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF) {}
            fprintf(stderr, "Command too long (maximum 2046 characters).\n");
            continue;
        }
        /* Pipelines belong to Lab C; do not silently execute only one side. */
        if (strchr(input, '|')) {
            fprintf(stderr, "Shell pipelines are not supported in Lab 2.\n");
            continue;
        }
        cmdLine *cmd = parseCmdLines(input);
        if (!cmd) continue;
        if (!cmd->argCount) {
            fprintf(stderr, "Missing command.\n");
            freeCmdLines(cmd);
            continue;
        }
        const char *name = cmd->arguments[0];
        if (!strcmp(name, "quit")) { freeCmdLines(cmd); break; }
        if (!strcmp(name, "cd")) {
            if (cmd->argCount != 2) fprintf(stderr, "Usage: cd <directory>\n");
            else if (chdir(cmd->arguments[1]) == -1) perror("cd");
        } else if (!strcmp(name, "alarm")) signal_command(cmd, SIGCONT);
        else if (!strcmp(name, "blast")) signal_command(cmd, SIGKILL);
        else {
            pid_t pid = fork();
            if (pid == 0) execute(cmd);
            if (pid < 0) perror("fork");
            else {
                if (debug) fprintf(stderr, "PID: %ld; Executing command: %s\n", (long)pid, name);
                if (cmd->blocking) {
                    /* The SIGCHLD handler may already have collected this child. */
                    while (waitpid(pid, NULL, 0) == -1) {
                        if (errno == EINTR) continue;
                        if (errno != ECHILD) perror("waitpid");
                        break;
                    }
                }
            }
        }
        freeCmdLines(cmd);
    }
    return EXIT_SUCCESS;
}
