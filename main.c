#include <err.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>

static void Pipe(int *pipefds);
static void Exec(char *prog);
static pid_t Fork(void);
static void Close(int fd);
static void Dup(int fd);
static void Run(char **cmds, int nr_cmds);

int
main(void)
{
        char *cmds[] = {
                "ls",
                "head",
                "wc",
        };
        int nr_cmds = sizeof(cmds) / sizeof(*cmds);

        Run(cmds, nr_cmds);
}

static void
Pipe(int *pipefds)
{
        if (pipe(pipefds))
                err(EX_OSERR, "pipe");
}

static void
Exec(char *prog)
{
        execlp(prog, prog, NULL);
        err(EX_OSERR, "execlp");
}

static pid_t
Fork(void)
{
        pid_t pid = fork();

        if (pid < 0)
                err(EX_OSERR, "fork");

        return pid;
}

static void
Close(int fd)
{
        if (close(fd))
                err(EX_OSERR, "close");
}

static void
Dup(int fd)
{
        if (dup(fd) < 0)
                err(EX_OSERR, "dup");
}

static void
Run(char **cmds, int nr_cmds)
{
        pid_t last;
        int first;
        int infd;
        int c;

        if (nr_cmds == 1) {
                last = Fork();
                if (last == 0)
                        Exec(cmds[0]);

                if (waitpid(last, NULL, 0) < 0)
                        err(EX_OSERR, "waitpid");

                return;
        }

        first = 1;
        infd = STDIN_FILENO;
        for (c = 0; c < nr_cmds; c++) {
                if (c == nr_cmds - 1) {
                        last = Fork();
                        if (last == 0) {
                                Close(STDIN_FILENO);

                                Dup(infd);

                                Close(infd);

                                Exec(cmds[c]);
                        }
                        Close(infd);
                } else if (first) {
                        int pipefds[2];

                        Pipe(pipefds);
                        if (Fork() == 0) {
                                Close(STDOUT_FILENO);

                                Dup(pipefds[1]);

                                Close(pipefds[0]);
                                Close(pipefds[1]);

                                Exec(cmds[c]);
                        }

                        infd = pipefds[0];
                        Close(pipefds[1]);
                        first = 0;
                } else {
                        int pipefds[2];

                        Pipe(pipefds);
                        if (Fork() == 0) {
                                Close(STDIN_FILENO);
                                Close(STDOUT_FILENO);

                                Dup(infd);
                                Dup(pipefds[1]);

                                Close(pipefds[0]);
                                Close(pipefds[1]);

                                Exec(cmds[c]);
                        }

                        infd = pipefds[0];
                        Close(pipefds[1]);
                }
        }

        if (waitpid(last, NULL, 0) < 0)
                err(EX_OSERR, "waitpid");
}
