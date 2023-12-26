#include <ctype.h>
#include <err.h>
#include <string.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct cmd {
        char *name;
        char **argv;
        int argc;
        int argp;
};

static void Pipe(int *pipefds);
static pid_t Fork(void);
static void Close(int fd);
static void Dup(int fd);
static void Run(struct cmd *cmds, size_t nr_cmds);
static void Execvp(char *file, char **argv);

int
main(void)
{
        char *line = NULL;
        size_t linelen = 0;
        size_t linecap = 0;
        int debug = 1;

        for (;;) {
                struct cmd *cmds = NULL;
                char *linep, *word;
                size_t cmdslen = 0;
                size_t cmdscap = 0;
                size_t i;
                int c;

                printf("> ");

                while ((c = getchar()) != EOF) {
                        if (linelen == linecap) {
                                linecap = linecap ? linecap * 2 : 1;
                                line = realloc(line,
                                        sizeof(*line) * (linecap + 1));
                                if (!line)
                                        err(EX_SOFTWARE, "realloc");
                        }
                        line[linelen++] = c;
                        if (c == '\n')
                                break;
                }
                if (c == EOF)
                        exit(0);

                line[linelen - 1] = '\0';
                if (debug) {
                        printf("\"%s\"\n", line);
                }

                cmdscap = 1;
                cmdslen = 1;
                cmds = malloc(sizeof(*cmds) * cmdscap);
                if (!cmds)
                        err(EX_SOFTWARE, "malloc");
                cmds[0].name = NULL;
                cmds[0].argv = NULL;
                cmds[0].argc = 0;
                cmds[0].argp = 0;

                linep = line;
                while ((word = strsep(&linep, " \n"))) {
                        struct cmd *p;

                        if (!*word)
                                continue;

                        if (debug) {
                                printf("\"%s\"\n", word);
                        }

                        p = &cmds[cmdslen - 1];

                        if (*word == '|') {
                                if (cmdslen == cmdscap) {
                                        cmdscap *= 2;
                                        cmds = realloc(cmds,
                                                sizeof(*cmds) * cmdscap);
                                        if (!cmds)
                                                err(EX_SOFTWARE, "realloc");
                                }
                                cmds[cmdslen].name = NULL;
                                cmds[cmdslen].argv = NULL;
                                cmds[cmdslen].argc = 0;
                                cmds[cmdslen].argp = 0;
                                cmdslen++;
                        } else {
                                if (!p->name) {
                                        p->name = strdup(word);
                                        if (!p->name)
                                                err(EX_SOFTWARE, "strdup");
                                }
                                p->argc++;
                                p->argv = realloc(p->argv,
                                        sizeof(*p->argv) * (p->argc + 1));
                                if (!p->argv)
                                        err(EX_SOFTWARE, "realloc");

                                p->argv[p->argp] = strdup(word);
                                if (!p->argv[p->argp])
                                        err(EX_SOFTWARE, "strdup");

                                p->argp++;
                                p->argv[p->argp] = NULL;
                        }
                }

                if (debug) {
                        for (i = 0; i < cmdslen; i++) {
                                struct cmd *p = &cmds[i];
                                size_t arg;

                                printf("[%zu] = {\n", i);
                                printf("\tname = \"%s\",\n", p->name);

                                printf("\targv = [\n");
                                for (arg = 0; arg < cmds[i].argp; arg++) {
                                        printf("\t\t[%zu] = \"%s\",\n",
                                                        arg, p->argv[arg]);
                                }
                                printf("\t\t[%zu] = %p\n", arg, p->argv[arg]);
                                printf("\t]\n");
                                printf("},\n");
                        }
                }

                Run(cmds, cmdslen);

                for (i = 0; i < cmdslen; i++) {
                        struct cmd *p = &cmds[i];
                        size_t arg;

                        free(p->name);
                        p->name = NULL;

                        for (arg = 0; arg < cmds[i].argp; arg++) {
                                free(p->argv[arg]);
                                p->argv[arg] = NULL;
                        }

                        free(p->argv);
                        p->argv = NULL;
                }
                free(cmds);
                cmds = NULL;

                linelen = 0;
        }
}

static void
Pipe(int *pipefds)
{
        if (pipe(pipefds))
                err(EX_OSERR, "pipe");
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
Run(struct cmd *cmds, size_t nr_cmds)
{
        pid_t last;
        int first;
        int infd;
        size_t c;

        if (nr_cmds == 1) {
                last = Fork();
                if (last == 0)
                        Execvp(cmds[0].name, cmds[0].argv);

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

                                Execvp(cmds[c].name, cmds[c].argv);
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

                                Execvp(cmds[c].name, cmds[c].argv);
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

                                Execvp(cmds[c].name, cmds[c].argv);
                        }

                        infd = pipefds[0];
                        Close(pipefds[1]);
                }
        }

        if (waitpid(last, NULL, 0) < 0)
                err(EX_OSERR, "waitpid");
}

static void
Execvp(char *file, char **argv)
{
        execvp(file, argv);
        err(EX_OSERR, "execvp");
}
