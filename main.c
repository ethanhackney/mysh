#include <err.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>

static void Pipe(int *pipefds);
static void Exec(char *prog);
static pid_t Fork(void);
static void Close(int fd);
static void Dup(int fd);

int
main(void)
{
        pid_t last;
        int pipefds[2];
        int pipefds2[2];
        int infd;

        /**
         * File descriptor table
         * =====================
         *
         * [ 0 ]----> stdin
         * [ 1 ]----> stdout
         * [ 2 ]----> stderr
         * [...]
         * [ p ]----> [ ][ ][ ][ ]
         * [ p ]-------^
         */
        Pipe(pipefds);

        if (Fork() == 0) {
                /**
                 * File descriptor table
                 * =====================
                 *
                 * [ 0 ]----> stdin
                 * [ 1 ]----x
                 * [ 2 ]----> stderr
                 * [...]
                 * [ p ]----> [ ][ ][ ][ ]
                 * [ p ]-------^
                 */
                Close(STDOUT_FILENO);

                /**
                 * File descriptor table
                 * =====================
                 *
                 * [ 0 ]----> stdin
                 * [ 1 ]----------------+
                 * [ 2 ]----> stderr    |
                 * [...]                V
                 * [ p ]----> [ ][ ][ ][ ]
                 * [ p ]-------^
                 */
                Dup(pipefds[1]);

                /**
                 * File descriptor table
                 * =====================
                 *
                 * [ 0 ]----> stdin
                 * [ 1 ]----------------+
                 * [ 2 ]----> stderr    |
                 * [...]                V
                 * [ p ]----x [ ][ ][ ][ ]
                 * [ p ]----x
                 */
                Close(pipefds[0]);
                Close(pipefds[1]);

                Exec("ls");
        }

        /**
         * File descriptor table
         * =====================
         *
         * [ 0 ]----> stdin
         * [ 1 ]----> stdout
         * [ 2 ]----> stderr
         * [...]
         * [ p ]----> [ ][ ][ ][ ]
         * [ p ]-------^
         * [ p ]----> [ ][ ][ ][ ]
         * [ p ]-------^
         */
        Pipe(pipefds2);

        if (Fork() == 0) {
                /**
                 * File descriptor table
                 * =====================
                 *
                 * [ 0 ]----x
                 * [ 1 ]----x
                 * [ 2 ]----> stderr
                 * [...]
                 * [ p ]----> [ ][ ][ ][ ]
                 * [ p ]-------^
                 * [ p ]----> [ ][ ][ ][ ]
                 * [ p ]-------^
                 */
                Close(STDIN_FILENO);
                Close(STDOUT_FILENO);

                /**
                 * File descriptor table
                 * =====================
                 *
                 * [ 0 ]----------------+
                 * [ 1 ]----------------|---+
                 * [ 2 ]----> stderr    |   |
                 * [...]       V--------+   |
                 * [ p ]----> [ ][ ][ ][ ]  |
                 * [ p ]-------^            |
                 * [ p ]----> [ ][ ][ ][ ]  |
                 * [ p ]-------^------------+
                 */
                Dup(pipefds[0]);
                Dup(pipefds2[1]);

                /**
                 * File descriptor table
                 * =====================
                 *
                 * [ 0 ]----------------+
                 * [ 1 ]----------------|---+
                 * [ 2 ]----> stderr    |   |
                 * [...]       V--------+   |
                 * [ p ]----x [ ][ ][ ][ ]  |
                 * [ p ]----x               |
                 * [ p ]----> [ ][ ][ ][ ]  |
                 * [ p ]-------^------------+
                 */
                Close(pipefds[0]);
                Close(pipefds[1]);

                /**
                 * File descriptor table
                 * =====================
                 *
                 * [ 0 ]----------------+
                 * [ 1 ]----------------|---+
                 * [ 2 ]----> stderr    |   |
                 * [...]       V--------+   |
                 * [ p ]----x [ ][ ][ ][ ]  |
                 * [ p ]----x               |
                 * [ p ]----x [ ][ ][ ][ ]  |
                 * [ p ]----x  ^------------+
                 */
                Close(pipefds2[0]);
                Close(pipefds2[1]);

                Exec("head");
        }

        /**
         * File descriptor table
         * =====================
         *
         * [ 0 ]----> stdin
         * [ 1 ]----> stdout
         * [ 2 ]----> stderr
         * [...]
         * [ p ]----x [ ][ ][ ][ ]
         * [ p ]----x
         * [ p ]----> [ ][ ][ ][ ]
         * [ p ]-------^
         */
        Close(pipefds[0]);
        Close(pipefds[1]);

        last = Fork();
        if (last == 0) {
                /**
                 * File descriptor table
                 * =====================
                 *
                 * [ 0 ]----x
                 * [ 1 ]----> stdout
                 * [ 2 ]----> stderr
                 * [...]
                 * [ p ]----x [ ][ ][ ][ ]
                 * [ p ]----x
                 * [ p ]----> [ ][ ][ ][ ]
                 * [ p ]-------^
                 */
                Close(STDIN_FILENO);

                /**
                 * File descriptor table
                 * =====================
                 *
                 * [ 0 ]----------------------+
                 * [ 1 ]----> stdout          |
                 * [ 2 ]----> stderr          |
                 * [...]                      |
                 * [ p ]----x [ ][ ][ ][ ]    |
                 * [ p ]----x  V--------------+
                 * [ p ]----> [ ][ ][ ][ ]
                 * [ p ]-------^
                 */
                Dup(pipefds2[0]);

                /**
                 * File descriptor table
                 * =====================
                 *
                 * [ 0 ]----------------------+
                 * [ 1 ]----> stdout          |
                 * [ 2 ]----> stderr          |
                 * [...]                      |
                 * [ p ]----x [ ][ ][ ][ ]    |
                 * [ p ]----x  V--------------+
                 * [ p ]----x [ ][ ][ ][ ]
                 * [ p ]----x
                 */
                Close(pipefds2[0]);
                Close(pipefds2[1]);

                Exec("wc");
        }

        /**
         * File descriptor table
         * =====================
         *
         * [ 0 ]----> stdin
         * [ 1 ]----> stdout
         * [ 2 ]----> stderr
         * [...]
         * [ p ]----x [ ][ ][ ][ ]
         * [ p ]----x
         * [ p ]----x [ ][ ][ ][ ]
         * [ p ]----x
         */
        Close(pipefds2[0]);
        Close(pipefds2[1]);

        if (waitpid(last, NULL, 0) < 0)
                err(EX_OSERR, "waitpid");
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
