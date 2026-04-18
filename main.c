typedef long size_t;

/* ---------------- syscalls ---------------- */
#define SYS_write   64
#define SYS_openat  56
#define SYS_close   57
#define SYS_dup3    24
#define SYS_execve  221
#define SYS_exit    93
#define SYS_access  21
#define SYS_ioctl   29
#define SYS_rt_sigaction 134

#define AT_FDCWD -100

#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT  0100
#define O_APPEND 02000

#define S_IRUSR 0400
#define S_IWUSR 0200

#define SIGHUP 1
#define SIG_IGN 1

struct kernel_sigaction {
    void (*handler)(int);
    unsigned long flags;
    void (*restorer)(void);
    unsigned long mask;
};

/* ---------------- syscall ---------------- */
static long syscall(long n,
    long a1,long a2,long a3,long a4,long a5,long a6)
{
    register long x8 asm("x8") = n;
    register long x0 asm("x0") = a1;
    register long x1 asm("x1") = a2;
    register long x2 asm("x2") = a3;
    register long x3 asm("x3") = a4;
    register long x4 asm("x4") = a5;
    register long x5 asm("x5") = a6;

    asm volatile("svc 0"
        : "=r"(x0)
        : "r"(x0),"r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5),"r"(x8)
        : "memory");

    return x0;
}

/* ---------------- utils ---------------- */
static size_t slen(const char *s)
{
    const char *p = s;
    while (*p) p++;
    return p - s;
}

static void w(int fd, const char *s)
{
    syscall(SYS_write, fd, (long)s, slen(s),0,0,0);
}

static int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *p = a;
    const unsigned char *q = b;

    while (n--) {
        if (*p != *q)
            return *p - *q;
        p++;
        q++;
    }
    return 0;
}

/* ---------------- getenv ---------------- */
static char *getenv_local(char **envp, const char *name)
{
    size_t nlen = slen(name);

    for (int i = 0; envp && envp[i]; i++) {
        if (memcmp(envp[i], name, nlen) == 0
            && envp[i][nlen] == '=') {
            return envp[i] + nlen + 1;
        }
    }
    return 0;
}

/* ---------------- string helpers ---------------- */
static int has_slash(const char *s)
{
    while (*s) {
        if (*s == '/') return 1;
        s++;
    }
    return 0;
}

/* ---------------- execve ---------------- */
static int my_execve(const char *path, char *const argv[], char *const envp[])
{
    return syscall(SYS_execve,
        (long)path,
        (long)argv,
        (long)envp,
        0,0,0);
}

/* ---------------- execvp (BusyBox-like) ---------------- */
static int my_execvp(const char *file, char *const argv[], char *const envp[])
{
    char buf[256];

    if (has_slash(file)) {
        my_execve(file, argv, envp);
        return -1;
    }

    char *path = getenv_local(envp, "PATH");
    if (!path) path = "/bin:/usr/bin";

    const char *p = path;

    while (1) {
        const char *start = p;

        while (*p && *p != ':') p++;

        int len = p - start;
        if (len == 0) {
            start = ".";
            len = 1;
        }

        char *d = buf;
        for (int i = 0; i < len && i < 200; i++)
            *d++ = start[i];

        *d++ = '/';

        const char *f = file;
        while (*f && (d - buf) < 250)
            *d++ = *f++;

        *d = 0;

        my_execve(buf, argv, envp);

        if (*p == 0) break;
        p++;
    }

    return -1;
}

static int isatty(int fd)
{
    char buf[32];
    return syscall(SYS_ioctl, fd, 0x5401, (long)buf, 0,0,0) >= 0;
}

int main(int argc, char **argv, char **envp)
{
    const char *out = "nohup.out";
    char out_buf[256];

    if (!argv[1]) {
        w(2, "usage: nohup PROG [ARGS]\n");
        return 127;
    }

    int in_tty  = isatty(0);
    int out_tty = isatty(1);
    int err_tty = isatty(2);

    if (in_tty) {
        syscall(SYS_close, 0,0,0,0,0,0);
        syscall(SYS_openat, AT_FDCWD, (long)"/dev/null", O_RDONLY,0,0,0);
    }

    int fd_out = -1;

    if (out_tty) {
        syscall(SYS_close, 1,0,0,0,0,0);

        fd_out = syscall(SYS_openat, AT_FDCWD,
            (long)out,
            O_CREAT | O_WRONLY | O_APPEND,
            0600,0,0);

        if (fd_out < 0) {
            char *home = getenv_local(envp, "HOME");

            if (home) {
                char *d = out_buf;
                out = out_buf;

                while (*home) *d++ = *home++;
                *d++ = '/';

                const char *s = "nohup.out";
                while (*s) *d++ = *s++;
                *d = 0;

                fd_out = syscall(SYS_openat, AT_FDCWD,
                    (long)out,
                    O_CREAT | O_WRONLY | O_APPEND,
                    0600,0,0);
            }
        }

        if (fd_out < 0) {
            fd_out = syscall(SYS_openat, AT_FDCWD,
                (long)"/dev/null",
                O_WRONLY,0,0,0);
            out = "/dev/null";
        }

        if (fd_out >= 0 && fd_out != 1) {
            syscall(SYS_dup3, fd_out, 1, 0,0,0,0);
            syscall(SYS_close, fd_out,0,0,0,0,0);
        }

        w(2, "appending output to ");
        w(2, out);
        w(2, "\n");
    }

    if (err_tty) {
        syscall(SYS_dup3, 1, 2, 0,0,0,0);
    }

    /* =========================
     * 4. SIGHUP ignore
     * ========================= */
    {
        struct kernel_sigaction sa;
        sa.handler = (void (*)(int))SIG_IGN;
        sa.flags = 0;
        sa.restorer = 0;
        sa.mask = 0;
        syscall(SYS_rt_sigaction, SIGHUP, (long)&sa, 0, sizeof(sa.mask),0,0);
    }

    /* =========================
     * 5. exec
     * ========================= */
    my_execvp(argv[1], &argv[1], envp);

    w(2, "exec failed\n");
    return 127;
}
