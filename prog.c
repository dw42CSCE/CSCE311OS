#include "prog.h"
#include "fs.h"
#include "apps.h"
#include "string.h"
#include "uart.h"
#include "thread.h"

typedef struct {
    int used;
    char name[PROG_NAME];
    char script[PROG_SCRIPT];
    int caps;
} user_prog;

static user_prog progs[PROG_MAX];

static int find_prog(const char *name) {
    for (int i = 0; i < PROG_MAX; ++i) {
        if (progs[i].used && strcmp(progs[i].name, name) == 0) return i;
    }
    return -1;
}

void prog_init(void) {
    for (int i = 0; i < PROG_MAX; ++i) {
        progs[i].used = 0;
        progs[i].name[0] = '\0';
        progs[i].script[0] = '\0';
        progs[i].caps = 0;
    }
}

int prog_load(const char *name, const char *script, int caps) {
    int idx = find_prog(name);
    if (idx < 0) {
        for (int i = 0; i < PROG_MAX; ++i) {
            if (!progs[i].used) { idx = i; break; }
        }
    }
    if (idx < 0) return -1;
    progs[idx].used = 1;
    strlcpy(progs[idx].name, name, PROG_NAME);
    strlcpy(progs[idx].script, script, PROG_SCRIPT);
    progs[idx].caps = caps;
    return 0;
}

int prog_load_file(const char *name, const char *file, int caps) {
    char buf[PROG_SCRIPT];
    if (fs_read(file, buf, sizeof(buf)) != 0) return -1;
    return prog_load(name, buf, caps);
}

int prog_drop(const char *name) {
    int idx = find_prog(name);
    if (idx < 0) return -1;
    progs[idx].used = 0;
    progs[idx].name[0] = '\0';
    progs[idx].script[0] = '\0';
    progs[idx].caps = 0;
    return 0;
}

int prog_save(const char *name, const char *file) {
    int idx = find_prog(name);
    if (idx < 0) return -1;
    return fs_write(file, progs[idx].script);
}

void prog_list(void) {
    uart_puts("user progs:\n");
    for (int i = 0; i < PROG_MAX; ++i) {
        if (progs[i].used) {
            uart_puts(" - ");
            uart_puts(progs[i].name);
            uart_puts(" caps:");
            char buf[16]; int n = 0;
            int v = progs[i].caps;
            char digits[8]; int d = 0;
            if (v == 0) digits[d++] = '0';
            while (v) { digits[d++] = '0' + (v % 10); v /= 10; }
            for (int k = d - 1; k >= 0; --k) buf[n++] = digits[k];
            buf[n] = '\0';
            uart_puts(buf);
            uart_puts("\n");
        }
    }
}

/* helpers for interpreter */
static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r') p++;
    return p;
}

static int take_word(const char **p, char *out, int max) {
    const char *s = skip_ws(*p);
    int n = 0;
    while (*s && *s != ' ' && *s != '\n' && *s != ';' && n + 1 < max) {
        out[n++] = *s++;
    }
    out[n] = '\0';
    *p = s;
    return n;
}

static int parse_int(const char *s) {
    int v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    return v;
}

static void prog_thread(void *arg) {
    user_prog *p = (user_prog *)arg;
    const char *pc = p->script;
    uart_puts("[prog:");
    uart_puts(p->name);
    uart_puts("] start\n");
    while (1) {
        pc = skip_ws(pc);
        if (!*pc) break;

        char word[32];
        take_word(&pc, word, sizeof(word));

        if (strcmp(word, "print") == 0) {
            if (!(p->caps & CAP_UART)) { uart_puts("[deny] print\n"); continue; }
            pc = skip_ws(pc);
            char line[128]; int n = 0;
            while (*pc && *pc != ';' && n + 1 < (int)sizeof(line)) {
                line[n++] = *pc++;
            }
            line[n] = '\0';
            uart_puts("[prog:");
            uart_puts(p->name);
            uart_puts("] ");
            uart_puts(line);
            uart_puts("\n");
        } else if (strcmp(word, "yield") == 0) {
            thread_yield();
        } else if (strcmp(word, "sleep") == 0) {
            pc = skip_ws(pc);
            char numbuf[16];
            take_word(&pc, numbuf, sizeof(numbuf));
            int n = parse_int(numbuf);
            if (n <= 0) n = 1;
            thread_sleep(n);
        } else if (strcmp(word, "spawn") == 0) {
            if (!(p->caps & CAP_SPAWN)) { uart_puts("[deny] spawn\n"); continue; }
            char name[32];
            take_word(&pc, name, sizeof(name));
            app_spawn(name);
        } else if (strcmp(word, "write") == 0) {
            if (!(p->caps & CAP_FS_W)) { uart_puts("[deny] write\n"); continue; }
            char fname[32];
            take_word(&pc, fname, sizeof(fname));
            pc = skip_ws(pc);
            char data[128]; int n = 0;
            while (*pc && *pc != ';' && n + 1 < (int)sizeof(data)) data[n++] = *pc++;
            data[n] = '\0';
            if (fs_write(fname, data) == 0) {
                uart_puts("[prog:");
                uart_puts(p->name);
                uart_puts("] wrote ");
                uart_puts(fname);
                uart_puts("\n");
            } else {
                uart_puts("[prog:");
                uart_puts(p->name);
                uart_puts("] write fail\n");
            }
        } else if (strcmp(word, "read") == 0) {
            if (!(p->caps & CAP_FS_R)) { uart_puts("[deny] read\n"); continue; }
            char fname[32];
            take_word(&pc, fname, sizeof(fname));
            char buf[128];
            if (fs_read(fname, buf, sizeof(buf)) == 0) {
                uart_puts("[prog:");
                uart_puts(p->name);
                uart_puts("] ");
                uart_puts(buf);
                uart_puts("\n");
            } else {
                uart_puts("[prog:");
                uart_puts(p->name);
                uart_puts("] read fail\n");
            }
        } else if (strcmp(word, "exit") == 0) {
            break;
        } else {
            uart_puts("[prog:");
            uart_puts(p->name);
            uart_puts("] unknown cmd\n");
        }

        while (*pc == ';') pc++;
    }
    uart_puts("[prog:");
    uart_puts(p->name);
    uart_puts("] exit\n");
}

int prog_run(const char *name) {
    int idx = find_prog(name);
    if (idx < 0) return -1;
    tid_t tid = thread_spawn(prog_thread, &progs[idx], name);
    return (int)tid;
}

int prog_run_all(void) {
    int started = 0;
    for (int i = 0; i < PROG_MAX; ++i) {
        if (progs[i].used) {
            thread_spawn(prog_thread, &progs[i], progs[i].name);
            started++;
        }
    }
    if (started == 0) return -1;
    return started;
}
