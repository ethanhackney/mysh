#include <string>
#include <vector>
#include <errno.h>
#include <cstdlib>
#include <cstdio>
#include <unordered_map>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <err.h>
#include <sysexits.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <cstring>
#include <cassert>
#include <cerrno>
#include <signal.h>

using namespace std;


/**
 * TODO:
 *
 * add while loops
 */

#define PROMPT "$"
#define PATH "/usr/local/pgsql/bin:/usr/local/go/bin:/home/ethanhackney/opt/cross/bin:/usr/local/pgsql/bin:/usr/local/go/bin:/home/ethanhackney/opt/cross/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin"

enum {
        HISTORY_LIMIT = 32,
};

enum {
        TOK_EOF,
        TOK_WORD,
        TOK_NUM,
        TOK_LT,
        TOK_GT,
        TOK_PIPE,
        TOK_IF,
        TOK_WHILE,
        TOK_LPAREN,
        TOK_RPAREN,
        TOK_RBRACE,
        TOK_LBRACE,
        TOK_NEWLINE,
        NR_TOK,
};

static const char *typenames[] = {
        "TOK_EOF",
        "TOK_WORD",
        "TOK_NUM",
        "TOK_LT",
        "TOK_GT",
        "TOK_PIPE",
        "TOK_IF",
        "TOK_WHILE",
        "TOK_LPAREN",
        "TOK_RPAREN",
        "TOK_RBRACE",
        "TOK_LBRACE",
        "TOK_NEWLINE",
};

struct token {
        string l;
        int t;
        int n;

        token(int type, string lex)
        {
                l = lex;
                t = type;
                if (type == TOK_NUM)
                        n = strtol(lex.c_str(), NULL, 10);
        }

        token(void) {}
};

enum {
        LEX_FILE = 0,
        LEX_STR,
};

unordered_map<string,string> symtab;

struct lexer {
        token curr;
        token p;
        string orig;
        int putback;
        FILE *fp;
        string s;
        int _type;
        size_t sp;

        string read_str(int delim)
        {
                string s;
                int c;

                while ((c = nextchar()) != EOF && c != '\n' && c != delim) {
                        if (c == '\\') {
                                s += nextchar();
                        } else if (c == '$' && delim != '\'') {
                                string var;
                                c = nextchar();
                                while (isalpha(c)) {
                                        var += c;
                                        c = nextchar();
                                }
                                s += symtab[var];
                                if (c == delim)
                                        break;
                        } else {
                                s += c;
                        }
                }
                if (c != delim)
                        putback = c;

                if (c != delim) {
                        printf("(malformed string)\n");
                        return "";
                }

                return s;
        }

        lexer(FILE *f, string src, int t)
        {
                s = src;
                fp = f;
                putback = 0;
                orig = "";
                _type = t;
                p.t = TOK_EOF;
                sp = 0;
                curr = next();
        }

        lexer() {}

        lexer(FILE *f, string src, int t, int pb)
        {
                s = src;
                fp = f;
                putback = pb;
                orig = "";
                _type = t;
                p.t = TOK_EOF;
                sp = 0;
                curr = next();
        }

        string lex(void)
        {
                return curr.l;
        }

        int num(void)
        {
                return curr.n;
        }

        int type(void)
        {
                return curr.t;
        }

        int nextchar(void)
        {
                int c;

                if (_type == LEX_FILE) {
                        c = putback;

                        if (c) {
                                putback = 0;
                        } else {
                                c = fgetc(fp);
                                if (c != '\n')
                                        orig += c;
                        }
                } else {
                        c = putback;

                        if (c) {
                                putback = 0;
                        } else {
                                if (sp == s.size())
                                        return EOF;
                                c = s[sp++];
                        }
                }

                return c;
        }

        int cmd_done(void)
        {
                return curr.t == TOK_EOF ||
                       curr.t == TOK_NEWLINE ||
                       curr.t == TOK_LPAREN ||
                       curr.t == TOK_RPAREN ||
                       curr.t == TOK_LBRACE ||
                       curr.t == TOK_RBRACE;
        }

        int done(void)
        {
                return curr.t == TOK_EOF || curr.t == TOK_NEWLINE;
        }

        token peek(void)
        {
                return p = next();
        }

        token next(void)
        {
                for (;;) {
                        auto c = nextchar();
                        string s;
                        bool n;

                        if (c == '\n')
                                return token(TOK_NEWLINE, "\n");

                        if (isspace(c))
                                continue;

                        switch (c) {
                        case '(':
                                return token(TOK_LPAREN, "(");
                        case ')':
                                return token(TOK_RPAREN, ")");
                        case '{':
                                return token(TOK_LBRACE, "{");
                        case '}':
                                return token(TOK_RBRACE, "}");
                        case EOF:
                                return token(TOK_EOF, "");
                        case '"':
                                return token(TOK_WORD, read_str('"'));
                        case '\'':
                                return token(TOK_WORD, read_str('\''));
                        case '|':
                                return token(TOK_PIPE, "|");
                        case '<':
                                return token(TOK_LT, "<");
                        case '>':
                                return token(TOK_GT, ">");
                        case '\\':
                                c = nextchar();
                                if (c == '\n')
                                        continue;
                                putback = c;
                                continue;
                        case '$':
                                c = nextchar();
                                while (isalpha(c)) {
                                        s += c;
                                        c = nextchar();
                                }
                                putback = c;
                                return token(TOK_WORD, symtab[s]);
                        case '#':
                                while ((c = nextchar()) != EOF && c != '\n')
                                        ;
                                if (c == EOF)
                                        return token(TOK_EOF, "");
                                return token(TOK_NEWLINE, "\n");
                        }

                        n = true;
                        while (isalpha(c) || isdigit(c) || c == '-' || c == '/') {
                                s += c;
                                if (!isdigit(c))
                                        n = false;
                                c = nextchar();
                        }
                        putback = c;

                        if (!n && isspace(c)) {
                                while (c != '\n' && isspace(c))
                                        c = nextchar();

                                if (c == '=') {
                                        while ((c = nextchar()) != '\n' && isspace(c))
                                                ;

                                        if (c == '\\' || c == '"') {
                                                string v;
                                                if (c == '\'') {
                                                        while ((c = nextchar()) != '\'') {
                                                                if (c == '\\')
                                                                        v += nextchar();
                                                                else
                                                                        v += c;
                                                        }
                                                        symtab[s] = v;
                                                        return next();
                                                } else if (c == '"') {
                                                        while ((c = nextchar()) != '"') {
                                                                if (c == '\\')
                                                                        v += nextchar();
                                                                else
                                                                        v += c;
                                                        }
                                                        symtab[s] = v;
                                                        return next();
                                                } else if (c == '\n') {
                                                        putback = c;
                                                }
                                        } else {
                                                putback = c;
                                        }

                                } else {
                                        putback = c;
                                }
                        } else {
                                putback = c;
                        }

                        if (s == "if")
                                return token(TOK_IF, s);
                        if (s == "while")
                                return token(TOK_WHILE, s);

                        if (n)
                                return token(TOK_NUM, s);

                        return token(TOK_WORD, s);
                }
        }

        void advance(void)
        {
                if (p.t != TOK_EOF) {
                        curr = p;
                        p.t = TOK_EOF;
                } else {
                        curr = next();
                }
        }

        void expect(int type)
        {
                if (curr.t != type)
                        err(EX_SOFTWARE, "got %s, wanted %s", typenames[curr.t], typenames[type]);
                advance();
        }
};

struct redirect {
        int fd;
        int dir;
        string path;
};

enum {
        TYPE_PIPE,
        TYPE_CMD,
        TYPE_IF,
        TYPE_WHILE,
};

struct ast {
        int type;
        vector<string> args;
        vector<redirect> dirs;
        ast *cond {nullptr};
        vector<ast*> stmts;
        ast *left {nullptr};
        ast *right {nullptr};
};

ast *stmt(lexer& l);
ast *if_stmt(lexer& l);
ast *while_stmt(lexer& l);
ast *expr(lexer& l);
ast *factor(lexer& l);
void ast_dump(ast *p, int space);
void indent(int space);
void exec_cmd(ast *ap, int infd, int outfd, vector<pid_t>& pids);
void ec(int fd, const char *fmt, ...);
void ed(int fd, const char *fmt, ...);
int eo(const char *path, int flags, mode_t mode, const char *fmt, ...);
void ast_free(ast *p);
void usage(const char *path);
int exec_stmt(ast *ap);
int exec_expr(ast *ap);

struct history_list {
        int wr;
        int rd;
        int sz;
        int len;
        vector<string> buf;

        history_list(int size)
        {
                if (size == 0)
                        return;
                wr = rd = len = 0;
                sz = size;
                buf.resize(sz);
        }

        void pop(void)
        {
                if (len == 0)
                        return;
                rd = (rd + 1) % sz;
                len--;
        }

        void push(string v)
        {
                if (sz == 0)
                        return;

                if (len == sz)
                        pop();

                buf[wr] = v;
                wr = (wr + 1) % sz;
                len++;
        }

        int ok(int i)
        {
                return i >= 0 && i < len;
        }

        string at(int i)
        {
                return buf[(rd + i) % sz];
        }

        void dump(void)
        {
                for (int i = 0; i < len; i++)
                        printf("[%d] => \"%s\"\n", i, at(i).c_str());
        }

        int length(void)
        {
                return len;
        }
};

void kill_fam(int sig);
void nothing(int sig) {}

FILE *logfp = NULL;
vector<pid_t> pids;

int main(int argc, char **argv)
{
        for (int i = 1; i < argc; i++) {
                if (strcmp(argv[i], "--help") == 0) {
                        usage(argv[0]);
                        exit(0);
                }
        }

        if (setenv("PATH", PATH, 0) < 0)
                err(EX_SOFTWARE, "setenv()");

        struct sigaction act;
        memset(&act, 0, sizeof(act));
        act.sa_flags = 0;
        sigemptyset(&act.sa_mask);
        act.sa_handler = SIG_IGN;
        if (sigaction(SIGINT, &act, NULL) < 0)
                err(EX_OSERR, "sigaction()");

        string logpath;
        FILE *sp = NULL;
        bool stdin_redir = false;
        string prompt = PROMPT;
        int hist_lim = HISTORY_LIMIT;
        bool script = false;

        int c;
        while ((c = getopt(argc, argv, "p:h:s:l:")) != -1) {
                switch (c) {
                case 'p':
                        prompt = optarg;
                        break;
                case 'h':
                        errno = 0;
                        hist_lim = strtol(optarg, NULL, 10);
                        if (errno) {
                                printf("bad history limit: %s\n", optarg);
                                exit(1);
                        }
                        break;
                case 's':
                        sp = fopen(optarg, "r");
                        if (!sp)
                                err(EX_SOFTWARE, "fopen()");
                        script = true;
                        break;
                case 'l':
                        logpath = optarg;
                        logfp = fopen(logpath.c_str(), "r");
                        if (!logfp) {
                                logfp = fopen(logpath.c_str(), "w+");
                                if (!logfp)
                                        err(EX_SOFTWARE, "fopen(%s)", logpath.c_str());
                        }
                        break;
                default:
                        usage(argv[0]);
                        exit(1);
                }
        }

        if (!isatty(fileno(stdin)))
                stdin_redir = true;

        history_list hist {hist_lim};
        for (;;) {
                if (!stdin_redir && !script)
                        printf("%s ", prompt.c_str());

                lexer l {script ? sp : stdin, "", LEX_FILE};

                if (l.lex() == "exit")
                        break;
                if (l.type() == TOK_EOF) {
                        if (!script && !stdin_redir)
                                puts("");
                        break;
                }
                if (l.type() == TOK_NEWLINE)
                        continue;

                ast *root;
                if (l.lex() == "history") {
                        if (l.peek().t == TOK_NEWLINE) {
                                hist.dump();
                                continue;
                        }

                        l.advance();
                        string arg = l.lex();
                        int i;
                        if (arg[0] == '-') {
                                i = (int)hist.length() - strtol(arg.c_str() + 1, 0, 10);
                        } else {
                                i = strtol(arg.c_str(), 0, 10);

                        }
                        while (!l.done())
                                l.advance();
                        if (l.type() != TOK_NEWLINE)
                                break;

                        if (!hist.ok(i)) {
                                printf("bad history argument: %s\n", arg.c_str());
                                continue;
                        }

                        l = lexer{NULL, hist.at(i), LEX_STR};
                        root = stmt(l);
                } else {
                        root = stmt(l);
                        if (l.type() != TOK_NEWLINE) {
                                ast_free(root);
                                puts("");
                                break;
                        }
                        hist.push(l.orig);
                }

                /* we need to do this because the child process
                 * messes with the file pointer
                 */
                long p;
                if (script)
                        p = ftell(sp);
                else if (stdin_redir)
                        p = ftell(stdin);

                (void)exec_stmt(root);

                if (script)
                        fseek(sp, p, SEEK_SET);
                else if (stdin_redir)
                        fseek(stdin, p, SEEK_SET);
        }
}

ast *stmt(lexer& l)
{
        if (l.type() == TOK_IF)
                return if_stmt(l);
        if (l.type() == TOK_WHILE)
                return while_stmt(l);
        return expr(l);
}

ast *if_stmt(lexer& l)
{
        auto *p = new ast{};

        p->type = TYPE_IF;
        l.expect(TOK_IF);

        l.expect(TOK_LPAREN);

        p->cond = stmt(l);

        l.expect(TOK_RPAREN);

        l.expect(TOK_LBRACE);
        while (l.type() != TOK_RBRACE)
                p->stmts.push_back(stmt(l));
        l.expect(TOK_RBRACE);

        return p;
}

ast *while_stmt(lexer& l)
{
        auto *p = new ast{};
        p->type = TYPE_WHILE;
        l.expect(TOK_IF);
        l.expect(TOK_LPAREN);
        p->cond = stmt(l);
        l.expect(TOK_RPAREN);
        l.expect(TOK_LBRACE);
        while (l.type() != TOK_RBRACE)
                p->stmts.push_back(stmt(l));
        l.expect(TOK_RBRACE);
        return p;
}

ast *expr(lexer& l)
{
        auto left = factor(l);

        while (!l.cmd_done() && l.type() == TOK_PIPE) {
                auto root = new ast{};
                l.advance();
                root->type = TYPE_PIPE;
                root->left = left;
                root->right = expr(l);
                left = root;
        }

        return left;
}

ast *factor(lexer& l)
{
        auto ap = new ast{};

        if (l.done())
                return NULL;

        auto w = l.lex();
        ap->args.push_back(w);
        ap->type = TYPE_CMD;
        l.advance();

        unordered_map<int,bool> set {
                { TOK_WORD, true },
                { TOK_NUM, true },
                { TOK_GT, true },
                { TOK_LT, true },
        };
        unordered_map<int,bool> dir {
                { TOK_GT, true },
                { TOK_LT, true },
        };

        while (!l.cmd_done() && set[l.type()]) {
                if (l.type() == TOK_NUM && dir[l.peek().t]) {
                        redirect r;

                        r.fd = l.num();
                        l.advance();

                        r.dir = l.lex()[0];
                        l.advance();

                        r.path = l.lex();
                        l.advance();

                        ap->dirs.push_back(r);
                } else if (dir[l.type()]) {
                        redirect r;

                        r.fd = -1;

                        r.dir = l.lex()[0];
                        l.advance();

                        r.path = l.lex();
                        l.advance();

                        ap->dirs.push_back(r);
                } else {
                        ap->args.push_back(l.lex());
                        l.advance();
                }
        }

        return ap;
}

void ast_dump(ast *p, int space)
{
        printf("{\n");

        indent(space + 2);
        printf("type: ");
        if (p->type == TYPE_PIPE || p->type == TYPE_CMD) {
                if (p->type == TYPE_PIPE)
                        printf("TYPE_PIPE");
                else if (p->type == TYPE_CMD)
                        printf("TYPE_CMD");
                printf(",\n");
        }

        if (p->type == TYPE_IF) {
                printf("TYPE_IF,\n");

                indent(space + 2);
                printf("cond: ");
                ast_dump(p->cond, space + 4);
                indent(space + 2);

                indent(space + 2);
                printf("stmts: [");
                for (auto s : p->stmts)
                        ast_dump(s, space + 4);
                indent(space + 2);
                printf("],\n");

                indent(space);
                printf("},\n");
                return;
        }

        if (p->type == TYPE_PIPE) {
                indent(space + 2);
                printf("left: ");
                ast_dump(p->left, space + 4);
                indent(space + 2);
                printf("},\n");

                indent(space + 2);
                printf("right: ");
                ast_dump(p->right, space + 4);
                indent(space + 2);
                printf("},\n");

                indent(space);
                printf("},\n");
                return;
        }

        indent(space + 2);
        printf("cmd: %s,\n", p->args[0].c_str());

        indent(space + 2);
        printf("args: [\n");
        for (auto a : p->args) {
                indent(space + 4);
                printf("%s,\n", a.c_str());
        }
        indent(space + 2);
        printf("],\n");

        indent(space + 2);
        printf("redirect: [\n");
        for (auto r : p->dirs) {
                indent(space + 4);
                printf("{\n");
                indent(space + 6);
                printf("fd: %d,\n", r.fd);
                indent(space + 6);
                printf("dir: %c,\n", r.dir);
                indent(space + 6);
                printf("path: %s,\n", r.path.c_str());
                indent(space + 4);
                printf("},\n");
        }
        indent(space + 2);
        printf("],\n");

        indent(space);
        printf("},\n");
}

void indent(int space)
{
        for (int i = 0; i < space; i++)
                printf(" ");
}

void exec_cmd(ast *ap, int infd, int outfd, vector<pid_t>& pids)
{
        if (ap->type == TYPE_PIPE) {
                int fd[2];
                if (pipe(fd) < 0)
                        err(EX_OSERR, "pipe");
                exec_cmd(ap->left, infd, fd[1], pids);
                ec(fd[1], "");
                exec_cmd(ap->right, fd[0], outfd, pids);
                ec(fd[0], "");
                delete ap;
                return;
        }

        auto args = new char *[ap->args.size() + 1];
        auto i = 0;
        for (auto a : ap->args) {
                auto dup = strdup(a.c_str());
                if (!dup)
                        err(EX_SOFTWARE, "strdup(%s)", a.c_str());
                args[i++] = dup;
        }
        args[i] = NULL;

        auto pid = fork();
        if (pid < 0)
                err(EX_OSERR, "fork");

        if (pid != 0) {
                for (size_t j = 0; j < ap->args.size(); j++)
                        free(args[j]);
                delete[] args;
                pids.push_back(pid);
                delete ap;
                return;
        }

        if (infd != 0) {
                ec(0, "");
                ed(infd, "");
                ec(infd, "");
        }

        if (outfd != 1) {
                ec(1, "");
                ed(outfd, "");
                ec(outfd, "");
        }

        for (auto r : ap->dirs) {
                if (r.fd == 2) {
                        auto fd = eo(r.path.c_str(), O_WRONLY | O_CREAT, 0666, "open(%s)", r.path.c_str());
                        ec(2, "");
                        ed(fd, "");
                        ec(fd, "");
                } else if (r.fd == -1 && r.dir == '<') {
                        auto fd = eo(r.path.c_str(), O_RDONLY | O_CREAT, 0666, "open(%s)", r.path.c_str());
                        ec(0, "");
                        ed(fd, "");
                        ec(fd, "");
                } else if (r.fd == -1 && r.dir == '>') {
                        auto fd = eo(r.path.c_str(), O_WRONLY | O_CREAT, 0666, "open(%s)", r.path.c_str());
                        ec(1, "");
                        ed(fd, "");
                        ec(fd, "");
                }
        }

        execvp(args[0], args);
        err(EX_OSERR, "execvp");
}

void ec(int fd, const char *fmt, ...)
{
        if (close(fd) == 0)
                return;

        va_list va;
        va_start(va, fmt);
        fprintf(logfp, fmt, va);
        va_end(va);
        fprintf(logfp, ": %s\n", strerror(errno));
        exit(1);
}

void ed(int fd, const char *fmt, ...)
{
        if (!(dup(fd) < 0))
                return;

        va_list va;
        va_start(va, fmt);
        fprintf(logfp, fmt, va);
        va_end(va);
        fprintf(logfp, ": %s\n", strerror(errno));
        exit(1);
}

int eo(const char *path, int flags, mode_t mode, const char *fmt, ...)
{
        auto fd = open(path, flags, mode);

        if (!(fd < 0))
                return fd;

        va_list va;
        va_start(va, fmt);
        fprintf(logfp, fmt, va);
        va_end(va);
        fprintf(logfp, ": %s\n", strerror(errno));
        exit(1);
        return -1;
}

void ast_free(ast *p)
{
        if (!p)
                return;

        ast_free(p->cond);
        for (auto s : p->stmts)
                ast_free(s);

        ast_free(p->left);
        ast_free(p->right);
        delete p;
}

void usage(const char *path)
{
        printf("USAGE: %s\n", path);
        printf("\tp: change prompt\n");
        printf("\th: change history limit\n");
        printf("\ts: read commands from file\n");
        printf("\tl: use log file\n");
}
void kill_fam(int sig)
{
        for (auto p : pids) {
                if (kill(p, SIGINT) < 0)
                        err(EX_OSERR, "kill()");
        }
        puts("");
        exit(5);
}

int exec_stmt(ast *ap)
{
        if (ap->type == TYPE_CMD || ap->type == TYPE_PIPE)
                return exec_expr(ap);

        if (ap->type == TYPE_IF) {
                int stat = exec_expr(ap->cond);
                if (stat == 0) {
                        for (auto s : ap->stmts)
                                stat = exec_stmt(s);
                } else {
                        for (auto s : ap->stmts)
                                ast_free(s);
                }
                delete ap;
                return stat;
        }

        return 0;
}

int exec_expr(ast *ap)
{
        struct sigaction act;
        int res = 0;

        auto pgid = fork();
        if (pgid < 0)
                err(EX_OSERR, "fork()");

        if (pgid == 0) {
                memset(&act, 0, sizeof(act));
                act.sa_flags = 0;
                sigemptyset(&act.sa_mask);
                act.sa_handler = kill_fam;
                if (sigaction(SIGINT, &act, NULL) < 0)
                        err(EX_OSERR, "sigaction()");
                vector<pid_t> pids;
                exec_cmd(ap, 0, 1, pids);
                int res = 0;
                for (size_t i = 0; i < pids.size(); i++) {
                        int status;
                        if (waitpid(pids[i], &status, 0) < 0)
                                err(EX_OSERR, "waitpid()");
                        if (status)
                                res = 1;
                        pids[i] = 0;
                }
                exit(res);
        }

        if (waitpid(pgid, &res, 0) < 0)
                err(EX_OSERR, "waitpid()");

        ast_free(ap);
        return res;
}
