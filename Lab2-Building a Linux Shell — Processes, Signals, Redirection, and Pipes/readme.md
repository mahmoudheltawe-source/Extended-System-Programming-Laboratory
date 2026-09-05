# Lab 2 — Simple Linux shell

The implementation is in `Lab2/`. It covers shell tasks 1–3, the task 0b signal helper, and the optional task 4 pipe exercise described in `Lab2-tasks`.

## Build and run

Requirements: Linux, GCC, and Make. From the directory containing this README:

```sh
make -C Lab2
cd Lab2
./myshell
```

Enable debug messages with `./myshell -d`. Debug output goes to stderr and includes the child PID and command name. Errors are reported to stderr even without debug mode, as required for failed commands and `cd`.

Type `quit` to exit. End-of-file (Ctrl+D on an empty terminal input line) also exits normally. To remove executables and object files, run `make clean` inside `Lab2/`.

## How the shell works

The shell displays the current directory, reads a command, and uses `LineParser` to parse it. External commands run in a child created with `fork`; `execvp` finds programs using `PATH`. The parent waits for foreground commands. A trailing `&` starts a background command without waiting.

A `SIGCHLD` handler collects exited children, including while the shell waits for input. This fixes the old README's background-process issue: a terminated looper is collected instead of remaining a zombie (`<defunct>`). A foreground wait also handles the case where the handler has already collected its child.

Built-in commands run in the shell process:

| Command | Behavior |
| --- | --- |
| `cd <directory>` | Changes the shell's working directory. |
| `alarm <pid>` | Sends `SIGCONT` to resume a stopped process. |
| `blast <pid>` | Sends `SIGKILL` to terminate a running or stopped process. |
| `quit` | Exits the shell. |

Signal commands require one positive numeric PID and report whether sending the signal succeeded. Obtain actual PIDs using `ps`; do not copy sample PID numbers. `alarm` sends a continue signal; it does not schedule a timer or interrupt an ordinary timed sleep.

External commands support `<` for input and `>` for output. Output redirection creates or truncates the file. Redirection happens only in the child, so the shell's prompt remains on the terminal.

Try these commands inside `myshell`:

```text
pwd
ls -l
echo hello > out.txt
cat < out.txt
sleep 2 &
cd ..
pwd
quit
```

## Test signals with looper

Start the shell from `Lab2/`, then enter:

```text
./looper&
./looper&
./looper&
ps
```

Using an actual looper PID shown by `ps`, enter `kill -TSTP PID` to stop it, `alarm PID` to resume it, and `blast PID` to terminate it. Replace `PID` with the number each time. Run `ps` again to inspect the result. Stop/resume can be repeated.

`looper` prints the names of received `SIGTSTP`, `SIGCONT`, and `SIGINT` signals and forwards them to their default handlers. It restores the opposite stop/continue handler so repeated cycles work. `SIGKILL` cannot be caught, so `blast` produces no signal message from looper.

Terminate all loopers with `blast` when finished. Exiting this shell does not automatically terminate background processes.

## Pipe exercise

From `Lab2/`, run:

```sh
./mypipe
```

The child writes `hello` through a pipe. The parent reads and prints it, closes its pipe endpoint, and waits for the child. Expected output:

```text
hello
```

## Scope and verification

This is a lab shell, not a full Bash replacement. It does not support quoting, wildcard expansion, variable expansion, append redirection, command lists, or full terminal job control. Shell pipelines are explicitly rejected; `mypipe` is a separate demonstration. Use simple space- or tab-separated arguments and place redirections after command arguments. Redirection applies to external commands, not built-ins. Input lines are limited to 2046 command characters plus a newline; longer lines are discarded with an error.

Verified with a warning-free `-Wall -Wextra -std=c11` build and functional checks for command execution, foreground completion, background execution, `cd`, redirection, EOF, invalid built-in arguments, debug output, pipe output, three simultaneous loopers, zombie cleanup, repeated stop/resume, and SIGINT termination.

`Lab2/Makefile` builds the sources inside `Lab2/`. The root-level `LineParser.c`, `LineParser.h`, and `looper.c` are duplicate helper sources; the parser whitespace fix and looper fix are also reflected there.

For the required shell submission, include `Lab2/myshell.c`, `Lab2/Makefile`, `Lab2/LineParser.c`, and `Lab2/LineParser.h`; compile that subset with `make myshell`. Include `looper.c` and `mypipe.c` as well if submitting the helper exercises or using the default `make` target. Do not include generated executables or object files.
