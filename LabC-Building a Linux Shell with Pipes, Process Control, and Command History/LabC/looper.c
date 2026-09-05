#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

/* Only async-signal-safe operations are used inside the handler. SIGSTOP
 * ensures the demonstration also stops in an orphaned process group. */
static void handler(int sig) {
    int saved_errno = errno;
    if (sig == SIGINT) {
        const char message[] = "Looper handling SIGINT\n";
        (void)write(STDOUT_FILENO, message, sizeof(message) - 1);
        _exit(128 + SIGINT);
    } else if (sig == SIGTSTP) {
        const char message[] = "Looper handling SIGTSTP\n";
        (void)write(STDOUT_FILENO, message, sizeof(message) - 1);
        kill(getpid(), SIGSTOP);
    } else if (sig == SIGCONT) {
        const char message[] = "Looper handling SIGCONT\n";
        (void)write(STDOUT_FILENO, message, sizeof(message) - 1);
    }
    errno = saved_errno;
}

int main(void) {
    struct sigaction action = {0};
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &action, NULL) < 0 ||
        sigaction(SIGTSTP, &action, NULL) < 0 ||
        sigaction(SIGCONT, &action, NULL) < 0) {
        perror("sigaction");
        return 1;
    }
    puts("Starting the program");
    fflush(stdout);
    for (;;) pause();
}
