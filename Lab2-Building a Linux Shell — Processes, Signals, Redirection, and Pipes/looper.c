#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <signal.h>
#include <string.h>

/* Precompute names: strsignal itself is not safe inside a signal handler. */
static const int signals[] = { SIGINT, SIGTSTP, SIGCONT };
static char messages[3][128];
static size_t lengths[3];

static void handler(int sig) {
    for (int i = 0; i < 3; ++i)
        if (sig == signals[i]) (void)write(STDOUT_FILENO, messages[i], lengths[i]);
    if (sig == SIGCONT) signal(SIGTSTP, handler);
    if (sig == SIGTSTP) signal(SIGCONT, handler);
    signal(sig, SIG_DFL);
    raise(sig);
}

int main(void) {
    for (int i = 0; i < 3; ++i) {
        strcpy(messages[i], "Received signal: ");
        strncat(messages[i], strsignal(signals[i]), sizeof(messages[i]) - 20);
        strcat(messages[i], "\n");
        lengths[i] = strlen(messages[i]);
        signal(signals[i], handler);
    }
    (void)write(STDOUT_FILENO, "Starting the program\n", 21);
    for (;;) pause();
}
