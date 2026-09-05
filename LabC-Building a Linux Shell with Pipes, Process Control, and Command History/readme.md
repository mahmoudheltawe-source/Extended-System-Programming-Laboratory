Lab C — shell, pipes, process management, and history

Status
------
The original code had functional problems despite the previous "working 100%"
readme. The code has been corrected against the requirements in LabC-tasks.
It now builds without warnings with GCC's -Wall -Wextra -Wpedantic flags.

Corrections include PATH lookup, parsing pipes and '&' without spaces, input
and output redirection, tracking both pipeline children, foreground completion
status, stop/continue notifications, and safe history recall. The looper now
handles repeated suspend/resume cycles. Failed system calls are reported, and
parsed commands/process records are freed on removal or shell exit.

Files
-----
LabC-tasks             Original assignment requirements.
LabC/myshell.c         Integrated shell for parts 2–4, plus cd and debug mode.
LabC/mypipeline.c      Independent part 1 program: ls -l | tail -n 2.
LabC/looper.c          Signal-handling program for process-manager experiments.
LabC/Makefile          Builds all three executables; includes test and clean.
LabC/tests/test_lab.py Integration tests using Python's standard library.

Build and run
-------------
Use Linux with GCC, make, and standard command-line utilities on PATH.
From the directory containing this readme:

    make -C LabC
    cd LabC
    ./mypipeline
    ./myshell

mypipeline prints the last two lines of the long directory listing to stdout.
Its required parent/child diagnostic messages go to stderr. To hide them:

    ./mypipeline 2>/dev/null

myshell shows the current directory as its interactive prompt. Enter commands
one at a time. Use quit or Ctrl-D at an empty prompt to exit. Debug mode prints
child PIDs and executable names to stderr:

    ./myshell -d

Batch input is also supported, without prompts:

    printf 'echo hello\nls|wc -l\nquit\n' | ./myshell

Shell commands and redirection
------------------------------
External commands use execvp, so both PATH commands and explicit paths work:

    ls -l
    /bin/echo hello
    cd /tmp
    cd

cd with no argument uses HOME. Quoted arguments, quoted filenames, and escaped
characters are supported. A trailing '&' launches an external command or an
entire pipeline in the background. Spaces around |, <, >, and & are optional.

    /bin/sleep 3&
    ls|wc -l
    printf 'one\ntwo\nthree\n' > in.txt
    cat < in.txt | tail -n 2 > out.txt
    cat out.txt

The last command prints "two" and "three" on separate lines. Output redirection
creates or truncates its file. Foreground commands wait until they exit or stop;
a pipeline starts both children before waiting, including for large outputs.

Only one pipe (two external commands) is supported, as required by the lab.
Redirecting the left command's output or the right command's input is rejected
before forking. Missing commands, missing redirection filenames, unmatched
quotes, and multiple pipes are also rejected.

Process manager
---------------
procs prints each tracked child's index, PID, status, and command arguments.
Both foreground/background commands and both pipeline children are tracked.
Terminated children are displayed once, then removed. Status updates use
waitpid with nonblocking, stopped, and continued event flags.

Built-ins, with a positive PID belonging to a live child of this shell:

    procs           Show and refresh the process list.
    sleep PID       Send SIGTSTP to suspend a process.
    suspend PID     Alias for sleep PID.
    alarm PID       Send SIGCONT to resume a process.
    wake PID        Alias for alarm PID.
    blast PID       Send SIGINT to request termination.
    kill PID        Alias for blast PID.

The assignment uses conflicting names: part 3c defines sleep as a PID command,
while earlier examples use the ordinary sleep utility. This shell follows 3c.
Use /bin/sleep SECONDS for a timed delay, not sleep SECONDS.

Example (replace PID below with the number shown by procs):

    ./looper&
    procs
    sleep PID
    procs
    alarm PID
    procs
    blast PID
    procs

looper prints a message for each SIGTSTP, SIGCONT, or SIGINT. Suspension and
termination statuses are confirmed from waitpid events, so an immediate procs
may precede the child's response; run procs again if necessary. To terminate
a stopped child, blast/kill also sends SIGCONT so it can handle pending SIGINT.
A program that catches or ignores SIGINT may remain alive and stays tracked.

Terminate background jobs before quitting if you do not want them to continue.
Shell exit frees its records but does not terminate remaining background jobs.

History
-------
The shell keeps the most recent 20 unparsed command lines in a circular queue.
Entries are numbered 1 through the current count, oldest to newest. Numbers
shift when the oldest entry is evicted. Each line may contain at most 199
characters, plus its terminating null byte; longer input is rejected in full.

    history         Display retained entries without adding this listing.
    !!              Re-execute the most recent retained command.
    !n              Re-execute entry n, for example !1.

Recall copies the selected text before inserting it again and parses it through
the same execution path. Pipes, redirection, and built-ins such as cd therefore
work when recalled. The expanded command is stored, not the !! or !n token.
Invalid history references print an error to stdout and do nothing. History is
kept only for the current shell session. Blank lines and syntax errors are not
stored.

Scope
-----
The supplied project has no LineParser files. myshell.c contains a self-contained
parser with cmdLine structures and the required process-list functions; no
additional parser download is needed. This is a lab shell, not a full POSIX
shell: it does not implement wildcard/variable expansion, command substitution,
append redirection, command lists, or terminal job control (fg/bg/process groups).
Built-ins must run alone, without pipes, redirection, or background execution.
Process commands only target children tracked by this shell.

Verification
------------
Python 3 is needed only for the tests. From the directory containing this readme:

    make -C LabC test

The eight integration tests cover the standalone pipeline, PATH execution,
quoting, large pipelines, foreground waiting, file redirection, invalid pipeline
rejection, process tracking/removal, background responsiveness, repeated signals,
termination of stopped jobs, history recall/wraparound, cd, invalid commands,
and overlong input.

The suite also passed against an AddressSanitizer/UndefinedBehaviorSanitizer
build of myshell with leak detection disabled. LeakSanitizer could not run in
the verification environment because of its tracing restrictions; leak freedom
is not claimed as dynamically verified.

To remove generated executables and object files:

    make -C LabC clean
