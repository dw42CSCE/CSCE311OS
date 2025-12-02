#include "uart.h"
#include <stddef.h>
#include "string.h"
#include "apps.h"
#include "fs.h"
#include "prog.h"
#include "thread.h"

/* tiny helpers for command parsing */
static const char *skip_space(const char *s) {
    while (*s == ' ') s++;
    return s;
}

static int read_word(const char **p, char *out, int max) {
    const char *s = skip_space(*p);
    int n = 0;
    while (*s && *s != ' ' && n + 1 < max) {
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

/* scheduler wrapper to guarantee a valid RA when main context is saved */
__attribute__((noinline))
static void sched_tick_wrapper(void) {
    sched_tick();
}

static void handle_fs(const char *args) {
    args = skip_space(args);
    if (!strcmp(args, "ls")) {
        fs_list();
        return;
    }
    if (!strcmp(args, "format")) {
        fs_format();
        uart_puts("fs formatted\n");
        return;
    }
    if (!strncmp(args, "read ", 5)) {
        char name[32];
        args += 5;
        read_word(&args, name, sizeof(name));
        char buf[128];
        if (fs_read(name, buf, sizeof(buf)) == 0) {
            uart_puts(buf);
            uart_puts("\n");
        } else {
            uart_puts("fs read failed\n");
        }
        return;
    }
    if (!strncmp(args, "write ", 6)) {
        char name[32];
        args += 6;
        read_word(&args, name, sizeof(name));
        args = skip_space(args);
        if (fs_write(name, args) == 0) {
            uart_puts("fs wrote ");
            uart_puts(name);
            uart_puts("\n");
        } else {
            uart_puts("fs write failed\n");
        }
        return;
    }
    if (!strncmp(args, "rm ", 3)) {
        char name[32];
        args += 3;
        read_word(&args, name, sizeof(name));
        if (fs_delete(name) == 0) uart_puts("fs removed\n");
        else uart_puts("fs rm failed\n");
        return;
    }
    uart_puts("fs usage: fs ls|format|read <f>|write <f> <data>|rm <f>\n");
}

static void handle_prog(const char *args) {
    args = skip_space(args);
    if (!strcmp(args, "ls")) { prog_list(); return; }
    if (!strcmp(args, "runall")) {
        int started = prog_run_all();
        if (started < 0) uart_puts("no progs\n");
        return;
    }
    if (!strncmp(args, "run ", 4)) {
        char name[32];
        args += 4;
        read_word(&args, name, sizeof(name));
        if (prog_run(name) < 0) uart_puts("no such prog\n");
        return;
    }
    if (!strncmp(args, "drop ", 5)) {
        char name[32];
        args += 5;
        read_word(&args, name, sizeof(name));
        if (prog_drop(name) == 0) uart_puts("prog dropped\n");
        else uart_puts("prog drop failed\n");
        return;
    }
    if (!strncmp(args, "load ", 5)) {
        char name[32], capsbuf[16];
        args += 5;
        read_word(&args, name, sizeof(name));
        read_word(&args, capsbuf, sizeof(capsbuf));
        int caps = parse_int(capsbuf);
        args = skip_space(args);
        if (prog_load(name, args, caps) == 0) uart_puts("prog loaded\n");
        else uart_puts("prog load failed\n");
        return;
    }
    if (!strncmp(args, "loadfile ", 9)) {
        char name[32], capsbuf[16], fname[32];
        args += 9;
        read_word(&args, name, sizeof(name));
        read_word(&args, capsbuf, sizeof(capsbuf));
        read_word(&args, fname, sizeof(fname));
        int caps = parse_int(capsbuf);
        if (prog_load_file(name, fname, caps) == 0) uart_puts("prog loaded from file\n");
        else uart_puts("prog loadfile failed\n");
        return;
    }
    if (!strncmp(args, "save ", 5)) {
        char name[32], fname[32];
        args += 5;
        read_word(&args, name, sizeof(name));
        read_word(&args, fname, sizeof(fname));
        if (prog_save(name, fname) == 0) uart_puts("prog saved\n");
        else uart_puts("prog save failed\n");
        return;
    }
    uart_puts("prog usage: prog ls|runall|load <name> <caps> <script>|loadfile <name> <caps> <file>|run <name>|drop <name>|save <name> <file>\n");
}

/* tiny shell: ... */
void kernel_main(void) {
    fs_init();
    prog_init();
    uart_puts("tiny-shell: type 'help' or 'stop'\n");
    char buf[80];
    int pos = 0;
    uart_puts("$ ");
    for (;;) {
        /* allow scheduler to run background threads */

    	if (!uart_haschar()) {
	    sched_tick_wrapper();
	}
        int c = uart_getc();
        if (c == '\r') c = '\n';
        if (c == '\n') {
            uart_puts("\n");
            buf[pos] = '\0';
            if (pos > 0) {
                if (!strcmp(buf, "help")) {
                    uart_puts("commands: help stop ls run <app> ps kill <tid>\n");
                    uart_puts("          fs ... (ls/read/write/rm/format)\n");
                    uart_puts("          prog ... (ls/runall/load/loadfile/save/run/drop)\n");
                } else if (!strncmp(buf, "run ", 4)) {
                    const char *name = buf + 4;
                    if (app_spawn(name) < 0) {
                        uart_puts("no such app\n");
                    }
                } else if (!strcmp(buf, "ls") || !strcmp(buf, "apps")) {
                    app_list();
                } else if (!strcmp(buf, "ps")) {
                    thread_list();
                } else if (!strncmp(buf, "fs ", 3)) {
                    handle_fs(buf + 3);
                } else if (!strncmp(buf, "prog ", 5)) {
                    handle_prog(buf + 5);
                } else if (!strncmp(buf, "kill ", 5)) {
                    const char *s = buf + 5;
                    int tid = 0;
                    while (*s >= '0' && *s <= '9') { tid = tid*10 + (*s - '0'); s++; }
                    if (thread_kill(tid) < 0) uart_puts("no such tid\n");
                } else if (!strcmp(buf, "stop")) {
                    uart_puts("stopping kernel — halting now.\n");
                    while (1) { asm volatile("wfi"); }
                } else {
                    uart_puts("unknown\n");
                }
            }
            pos = 0;
            uart_puts("$ ");
        } else if (c == 8 || c == 127) { // backspace
            if (pos > 0) {
                pos--;
                uart_puts("\b \b");
            }
        } else {
            if (pos < (int)sizeof(buf)-1) {
                buf[pos++] = (char)c;
                uart_putc((char)c);
            }
        }
    }
}
