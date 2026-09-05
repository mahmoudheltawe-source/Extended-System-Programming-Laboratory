#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>

#define MAX_ARGUMENTS 256
#define HISTLEN 20
#define MAX_BUF 200
#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0

/* Self-contained equivalent of the lab's parsed command representation. */
typedef struct cmdLine {
    char *arguments[MAX_ARGUMENTS];
    int argCount;
    char *inputRedirect, *outputRedirect;
    int blocking;
    struct cmdLine *next;
} cmdLine;

typedef struct process {
    cmdLine *cmd;
    pid_t pid;
    int status;
    struct process *next;
} process;

static process *process_list;
static char history[HISTLEN][MAX_BUF];
static int newest, oldest, history_count, debug;

static void *allocate(size_t size) {
    void *p = calloc(1, size);
    if (!p) { perror("calloc"); exit(EXIT_FAILURE); }
    return p;
}

static char *copyString(const char *s) {
    char *p = strdup(s);
    if (!p) { perror("strdup"); exit(EXIT_FAILURE); }
    return p;
}

static void freeCmdLines(cmdLine *cmd) {
    while (cmd) {
        cmdLine *next = cmd->next;
        for (int i = 0; i < cmd->argCount; ++i) free(cmd->arguments[i]);
        free(cmd->inputRedirect);
        free(cmd->outputRedirect);
        free(cmd);
        cmd = next;
    }
}

/* Operators are recognized even without spaces; quotes and backslashes keep
 * literal characters inside a word. No variable or wildcard expansion. */
static int nextToken(const char **cursor, char *word) {
    const char *p = *cursor;
    while (isspace((unsigned char)*p)) ++p;
    if (!*p) { *cursor = p; return 0; }
    if (strchr("|<>&", *p)) { *cursor = p + 1; return *p; }
    int n = 0;
    while (*p && !isspace((unsigned char)*p) && !strchr("|<>&", *p)) {
        if (*p == '\'' || *p == '"') {
            char quote = *p++;
            while (*p && *p != quote) {
                if (quote == '"' && *p == '\\' && p[1] &&
                    (p[1] == '"' || p[1] == '\\')) ++p;
                word[n++] = *p++;
            }
            if (!*p) return -1;
            ++p;
        } else if (*p == '\\') {
            if (!*++p) return -1;
            word[n++] = *p++;
        } else word[n++] = *p++;
    }
    word[n] = '\0';
    *cursor = p;
    return 'w';
}

static cmdLine *parseCmdLines(const char *line) {
    cmdLine *head = allocate(sizeof(*head)), *cmd = head;
    head->blocking = 1;
    char word[MAX_BUF];
    int token;
    while ((token = nextToken(&line, word)) != 0) {
        if (token == 'w') {
            if (cmd->argCount == MAX_ARGUMENTS - 1) goto invalid;
            cmd->arguments[cmd->argCount++] = copyString(word);
        } else if (token == '<' || token == '>') {
            char **target = token == '<' ? &cmd->inputRedirect : &cmd->outputRedirect;
            if (*target || nextToken(&line, word) != 'w' || !word[0]) goto invalid;
            *target = copyString(word);
        } else if (token == '|') {
            if (!cmd->argCount || head->next) goto invalid;
            cmd->next = allocate(sizeof(*cmd));
            cmd = cmd->next;
            cmd->blocking = 1;
        } else if (token == '&') {
            if (nextToken(&line, word) != 0) goto invalid;
            head->blocking = cmd->blocking = 0;
            break;
        } else goto invalid;
    }
    if (!cmd->argCount || !cmd->arguments[0][0]) goto invalid;
    if (head->next && (head->outputRedirect || head->next->inputRedirect)) {
        fprintf(stderr, "Invalid pipeline: left output and right input cannot be redirected.\n");
        freeCmdLines(head);
        return NULL;
    }
    return head;
invalid:
    fprintf(stderr, "Invalid command syntax (at most one pipe is supported).\n");
    freeCmdLines(head);
    return NULL;
}

void addProcess(process **list, cmdLine *cmd, pid_t pid) {
    process *entry = allocate(sizeof(*entry));
    entry->cmd = cmd;
    entry->pid = pid;
    entry->status = RUNNING;
    while (*list) list = &(*list)->next;
    *list = entry;
}

void updateProcessStatus(process *list, int pid, int status) {
    for (; list; list = list->next)
        if (list->pid == pid) { list->status = status; return; }
}

static void recordStatus(pid_t pid, int status) {
    if (WIFEXITED(status) || WIFSIGNALED(status))
        updateProcessStatus(process_list, pid, TERMINATED);
    else if (WIFSTOPPED(status)) updateProcessStatus(process_list, pid, SUSPENDED);
    else if (WIFCONTINUED(status)) updateProcessStatus(process_list, pid, RUNNING);
}

void updateProcessList(process **list) {
    for (process *p = *list; p; p = p->next) {
        if (p->status == TERMINATED) continue;
        int status;
        pid_t result;
        /* Drain pending stop/continue/exit notifications without blocking. */
        while ((result = waitpid(p->pid, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0)
            recordStatus(p->pid, status);
        if (result == -1 && errno == ECHILD) p->status = TERMINATED;
    }
}

void printProcessList(process **list) {
    updateProcessList(list);
    printf("INDEX\tPID\tSTATUS\t\tCOMMAND\n");
    int index = 1;
    while (*list) {
        process *p = *list;
        printf("%d\t%ld\t%s\t", index++, (long)p->pid,
               p->status == TERMINATED ? "Terminated" :
               p->status == SUSPENDED ? "Suspended" : "Running");
        for (int i = 0; i < p->cmd->argCount; ++i)
            printf("%s%s", i ? " " : "", p->cmd->arguments[i]);
        putchar('\n');
        if (p->status == TERMINATED) {
            *list = p->next;
            freeCmdLines(p->cmd);
            free(p);
        } else list = &p->next;
    }
}

void freeProcessList(process *list) {
    while (list) {
        process *next = list->next;
        freeCmdLines(list->cmd);
        free(list);
        list = next;
    }
}

static void addToHistory(const char *line) {
    strcpy(history[newest], line); /* Input is validated to fit MAX_BUF. */
    newest = (newest + 1) % HISTLEN;
    if (history_count < HISTLEN) ++history_count;
    else oldest = (oldest + 1) % HISTLEN;
}

static void printHistory(void) {
    for (int i = 0; i < history_count; ++i)
        printf("%d %s\n", i + 1, history[(oldest + i) % HISTLEN]);
}

static void waitForeground(pid_t pid) {
    int status;
    pid_t result;
    do { result = waitpid(pid, &status, WUNTRACED); } while (result < 0 && errno == EINTR);
    if (result > 0) recordStatus(pid, status);
    else if (result < 0) perror("waitpid");
}

static void redirectFile(const char *path, int target) {
    if (!path) return;
    int fd = open(path, target == STDIN_FILENO ? O_RDONLY : O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) { perror(path); _exit(EXIT_FAILURE); }
    if (dup2(fd, target) < 0) { perror("dup2"); _exit(EXIT_FAILURE); }
    if (fd != target) close(fd);
}

static void runChild(cmdLine *cmd, int input, int output, int pipefd[2]) {
    if (input >= 0 && dup2(input, STDIN_FILENO) < 0) { perror("dup2"); _exit(1); }
    if (output >= 0 && dup2(output, STDOUT_FILENO) < 0) { perror("dup2"); _exit(1); }
    if (pipefd) { close(pipefd[0]); close(pipefd[1]); }
    redirectFile(cmd->inputRedirect, STDIN_FILENO);
    redirectFile(cmd->outputRedirect, STDOUT_FILENO);
    execvp(cmd->arguments[0], cmd->arguments);
    perror(cmd->arguments[0]);
    _exit(127);
}

static void executeExternal(cmdLine *left) {
    cmdLine *right = left->next;
    int blocking = left->blocking, pipefd[2] = {-1, -1};
    if (right && pipe(pipefd) < 0) { perror("pipe"); freeCmdLines(left); return; }
    fflush(NULL);
    pid_t first = fork();
    if (first == 0) runChild(left, -1, right ? pipefd[1] : -1, right ? pipefd : NULL);
    if (first < 0) {
        perror("fork");
        if (right) { close(pipefd[0]); close(pipefd[1]); }
        freeCmdLines(left);
        return;
    }
    left->next = NULL; /* Each process now owns exactly one parsed command. */
    addProcess(&process_list, left, first);
    if (debug) fprintf(stderr, "PID: %ld Executing: %s\n", (long)first, left->arguments[0]);
    if (right) {
        pid_t second = fork();
        if (second == 0) runChild(right, pipefd[0], -1, pipefd);
        close(pipefd[0]);
        close(pipefd[1]);
        if (second < 0) {
            perror("fork");
            kill(first, SIGKILL);
            waitForeground(first);
            freeCmdLines(right);
            return;
        }
        addProcess(&process_list, right, second);
        if (debug) fprintf(stderr, "PID: %ld Executing: %s\n", (long)second, right->arguments[0]);
        if (blocking) { waitForeground(first); waitForeground(second); }
    } else if (blocking) waitForeground(first);
}

static int builtin(const char *name) {
    const char *names[] = {"quit", "cd", "history", "procs", "sleep", "suspend",
                           "alarm", "wake", "blast", "kill", NULL};
    for (int i = 0; names[i]; ++i) if (!strcmp(name, names[i])) return 1;
    return 0;
}

/* Return true only for quit. The caller owns and frees built-in commands. */
static int executeBuiltin(cmdLine *cmd) {
    const char *name = cmd->arguments[0];
    if (cmd->inputRedirect || cmd->outputRedirect || !cmd->blocking) {
        fprintf(stderr, "Built-ins do not support redirection or background execution.\n");
        return 0;
    }
    if (!strcmp(name, "quit") || !strcmp(name, "history") || !strcmp(name, "procs")) {
        if (cmd->argCount != 1) { fprintf(stderr, "%s takes no arguments.\n", name); return 0; }
        if (!strcmp(name, "quit")) return 1;
        if (!strcmp(name, "history")) printHistory();
        else printProcessList(&process_list);
    } else if (!strcmp(name, "cd")) {
        const char *path = cmd->argCount == 1 ? getenv("HOME") : cmd->arguments[1];
        if (cmd->argCount > 2 || !path) fprintf(stderr, "Usage: cd [directory]\n");
        else if (chdir(path) < 0) perror("cd");
    } else {
        char *end;
        errno = 0;
        long pid = cmd->argCount == 2 ? strtol(cmd->arguments[1], &end, 10) : 0;
        if (cmd->argCount != 2 || errno || pid <= 0 || pid > INT_MAX || *end) {
            fprintf(stderr, "Usage: %s <positive process id>\n", name);
            return 0;
        }
        updateProcessList(&process_list);
        process *p = process_list;
        while (p && p->pid != pid) p = p->next;
        if (!p || p->status == TERMINATED) {
            fprintf(stderr, "No live tracked process with id %ld.\n", pid);
            return 0;
        }
        int sig = (!strcmp(name, "sleep") || !strcmp(name, "suspend")) ? SIGTSTP :
                  (!strcmp(name, "alarm") || !strcmp(name, "wake")) ? SIGCONT : SIGINT;
        if (kill((pid_t)pid, sig) < 0) perror("kill");
        else {
            printf("Sent %s to %ld.\n", sig == SIGTSTP ? "SIGTSTP" : sig == SIGCONT ? "SIGCONT" : "SIGINT", pid);
            if (sig == SIGCONT) updateProcessStatus(process_list, (int)pid, RUNNING);
            /* A stopped child cannot handle SIGINT until it is continued. */
            if (sig == SIGINT && p->status == SUSPENDED && kill((pid_t)pid, SIGCONT) < 0)
                perror("SIGCONT after SIGINT");
            /* A signal may be caught or ignored. waitpid determines suspension
             * and termination; never discard a child before it is reaped. */
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "-d")) debug = 1;
    else if (argc != 1) { fprintf(stderr, "Usage: %s [-d]\n", argv[0]); return 1; }
    char input[MAX_BUF];
    int done = 0;
    while (!done) {
        updateProcessList(&process_list);
        if (isatty(STDIN_FILENO)) {
            char cwd[PATH_MAX];
            printf("%s> ", getcwd(cwd, sizeof(cwd)) ? cwd : "myshell");
            fflush(stdout);
        }
        if (!fgets(input, sizeof(input), stdin)) break;
        size_t len = strlen(input);
        if (len && input[len - 1] == '\n') input[--len] = '\0';
        else {
            int c = getchar();
            if (c != '\n' && c != EOF) {
                while ((c = getchar()) != '\n' && c != EOF) {}
                fprintf(stderr, "Command too long (maximum %d characters).\n", MAX_BUF - 1);
                continue;
            }
        }
        char *line = input;
        while (isspace((unsigned char)*line)) ++line;
        if (!*line) continue;
        if (*line == '!') {
            char *end;
            long index;
            if (line[1] == '!' && (line[2] == '\0' || isspace((unsigned char)line[2]))) {
                index = history_count;
                end = line + 2;
            } else {
                errno = 0;
                index = strtol(line + 1, &end, 10);
                if (errno || !isdigit((unsigned char)line[1])) index = 0;
            }
            while (isspace((unsigned char)*end)) ++end;
            if (*end || index < 1 || index > history_count) {
                printf("No such command in history.\n");
                continue;
            }
            strcpy(input, history[(oldest + (int)index - 1) % HISTLEN]);
            line = input;
            printf("%s\n", line);
            fflush(stdout);
        }
        cmdLine *cmd = parseCmdLines(line);
        if (!cmd) continue;
        /* Store expanded commands, never !!/!n or the history listing itself. */
        if (cmd->next || strcmp(cmd->arguments[0], "history")) addToHistory(input);
        if (cmd->next && (builtin(cmd->arguments[0]) || builtin(cmd->next->arguments[0]))) {
            fprintf(stderr, "Built-ins cannot be used in pipelines.\n");
            freeCmdLines(cmd);
        } else if (builtin(cmd->arguments[0])) {
            done = executeBuiltin(cmd);
            freeCmdLines(cmd);
        } else executeExternal(cmd);
    }
    freeProcessList(process_list);
    return 0;
}
