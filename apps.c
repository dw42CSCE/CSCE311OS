#include "uart.h"
#include "string.h"
#include "apps.h"
#include "thread.h"
#include "sync.h"
#include "fs.h"
#include "prog.h"
#include <stddef.h>

/* Simple built-in apps. Each app is a function that returns. */

static void app_hello(void) {
    uart_puts("[app:hello] Hello from built-in app!\n");
}

static void app_echo(void) {
    uart_puts("[app:echo] echoing... done\n");
}

/* an app that prints "ping" then yields many times */
static void app_pinger(void *unused) {
    (void)unused;
    for (int i = 0; i < 20; ++i) {
        uart_puts("[app:pinger] ping\n");
        /* give other threads a chance */
        thread_yield();
    }
    uart_puts("[app:pinger] done\n");
}

/* an app that counts numbers and yields each time */
static void app_counter(void *unused) {
    (void)unused;
    for (int i = 1; i <= 20; ++i) {
        char buf[32]; int n = 0;
        const char *p = "[app:counter] ";
        while (*p) buf[n++] = *p++;
        int v = i;
        char digits[16]; int d = 0;
        if (v == 0) digits[d++] = '0';
        while (v) { digits[d++] = '0' + (v % 10); v /= 10; }
        for (int k = d - 1; k >= 0; --k) buf[n++] = digits[k];
        buf[n++] = '\n';
        buf[n] = '\0';
        uart_puts(buf);

        thread_yield();
    }
    uart_puts("[app:counter] done\n");
}

/* demo of concurrent producer/consumer using semaphore + mutex */
typedef struct {
    char slots[4];
    int head;
    int tail;
    mutex_t lock;
    semaphore_t items;
    semaphore_t spaces;
} pc_state_t;

static pc_state_t pc_state;

static void producer(void *unused) {
    const char payload[] = { 'A', 'B', 'C', 'D', 'E', 'F' };
    for (int i = 0; i < 6; ++i) {
        sem_wait(&pc_state.spaces);
        mutex_lock(&pc_state.lock);
        pc_state.slots[pc_state.tail] = payload[i];
        pc_state.tail = (pc_state.tail + 1) % 4;
        mutex_unlock(&pc_state.lock);
        sem_post(&pc_state.items);
        uart_puts("[producer] queued item\n");
        thread_yield();
    }
    uart_puts("[producer] done\n");
}

static void consumer(void *unused) {
    for (int i = 0; i < 6; ++i) {
        sem_wait(&pc_state.items);
        mutex_lock(&pc_state.lock);
        char item = pc_state.slots[pc_state.head];
        pc_state.head = (pc_state.head + 1) % 4;
        mutex_unlock(&pc_state.lock);
        sem_post(&pc_state.spaces);
        uart_puts("[consumer] got ");
        char buf[4]; buf[0] = item; buf[1] = '\n'; buf[2] = '\0';
        uart_puts(buf);
        thread_yield();
    }
    uart_puts("[consumer] done\n");
}

static void app_syncdemo(void) {
    mutex_init(&pc_state.lock);
    sem_init(&pc_state.items, 0);
    sem_init(&pc_state.spaces, 4);
    pc_state.head = pc_state.tail = 0;

    thread_spawn(producer, NULL, "producer");
    thread_spawn(consumer, NULL, "consumer");
    uart_puts("[app:syncdemo] spawned producer/consumer\n");
}

static void app_fs_demo(void) {
    fs_write("hello.txt", "hi-from-fs");
    char buf[64];
    if (fs_read("hello.txt", buf, sizeof(buf)) == 0) {
        uart_puts("[app:fs] read back: ");
        uart_puts(buf);
        uart_puts("\n");
    }
}

static void app_prog_demo(void) {
    const char *script = "print script boot;write note hi!;read note;spawn pinger;yield;print bye;exit";
    prog_load("script1", script, CAP_UART | CAP_FS_W | CAP_FS_R | CAP_SPAWN);
    prog_run("script1");
}

static void sleepy_worker(void *arg) {
    int id = (int)(long)arg;
    for (int i = 0; i < 3; ++i) {
        char buf[48]; int n = 0;
        const char *p = "[sleepy ";
        while (*p) buf[n++] = *p++;
        /* id */
        int v = id;
        char digits[8]; int d = 0;
        if (v == 0) digits[d++] = '0';
        while (v) { digits[d++] = '0' + (v % 10); v /= 10; }
        for (int k = d - 1; k >= 0; --k) buf[n++] = digits[k];
        p = "] round ";
        while (*p) buf[n++] = *p++;
        v = i;
        d = 0;
        if (v == 0) digits[d++] = '0';
        while (v) { digits[d++] = '0' + (v % 10); v /= 10; }
        for (int k = d - 1; k >= 0; --k) buf[n++] = digits[k];
        buf[n++] = '\n';
        buf[n] = '\0';
        uart_puts(buf);

        /* stagger sleeps with different durations */
        thread_sleep(1 + id);
    }
    uart_puts("[sleepy] done\n");
}

static void app_sleepers(void) {
    for (int i = 0; i < 3; ++i) {
        thread_spawn(sleepy_worker, (void *)(long)i, "sleepy");
    }
    uart_puts("[app:sleepers] spawned sleepy threads\n");
}

static barrier_t sync_barrier;

static void barrier_worker(void *arg) {
    int id = (int)(long)arg;
    for (int step = 0; step < 3; ++step) {
        char buf[64]; int n = 0;
        const char *p = "[barrier worker ";
        while (*p) buf[n++] = *p++;
        int v = id;
        char digits[8]; int d = 0;
        if (v == 0) digits[d++] = '0';
        while (v) { digits[d++] = '0' + (v % 10); v /= 10; }
        for (int k = d - 1; k >= 0; --k) buf[n++] = digits[k];
        p = "] step ";
        while (*p) buf[n++] = *p++;
        v = step;
        d = 0;
        if (v == 0) digits[d++] = '0';
        while (v) { digits[d++] = '0' + (v % 10); v /= 10; }
        for (int k = d - 1; k >= 0; --k) buf[n++] = digits[k];
        buf[n++] = '\n';
        buf[n] = '\0';
        uart_puts(buf);

        barrier_wait(&sync_barrier);
        thread_sleep(1 + id);
    }
    uart_puts("[barrier worker] done\n");
}

static void app_barrier_demo(void) {
    barrier_init(&sync_barrier, 3);
    thread_spawn(barrier_worker, (void *)0, "bar0");
    thread_spawn(barrier_worker, (void *)1, "bar1");
    thread_spawn(barrier_worker, (void *)2, "bar2");
    uart_puts("[app:barrier] 3 workers waiting on barrier\n");
}

static void app_prog_file_demo(void) {
    fs_write("fileprog.txt", "print from-file;yield;spawn counter;exit");
    prog_load_file("fileprog", "fileprog.txt", CAP_UART | CAP_SPAWN);
    prog_run("fileprog");
}

static void app_sum(void) {
    int s = 0;
    for (int i = 1; i <= 10; ++i) s += i;
    /* print sum using a tiny int->string helper */
    char buf[32];
    int n = 0;
    const char *p = "sum=";
    while (*p) buf[n++] = *p++;
    int v = s;
    char digits[16]; int d = 0;
    if (v == 0) digits[d++] = '0';
    while (v) { digits[d++] = '0' + (v % 10); v /= 10; }
    for (int i = d - 1; i >= 0; --i) buf[n++] = digits[i];
    buf[n++] = '\n';
    buf[n] = '\0';
    uart_puts(buf);
}

typedef void (*app_fn)(void);
typedef struct { const char *name; app_fn fn; } app_entry;

static app_entry apps[] = {
    { "hello", app_hello },
    { "echo",  app_echo  },
    { "sum",   app_sum   },
    { "pinger", (app_fn)app_pinger },
    { "counter", (app_fn)app_counter },
    { "sync", app_syncdemo },
    { "fs-demo", app_fs_demo },
    { "prog-demo", app_prog_demo },
    { "sleepers", app_sleepers },
    { "barrier", app_barrier_demo },
    { "prog-file", app_prog_file_demo },

    { NULL, NULL }
};

int app_run(const char *name) {
    for (int i = 0; apps[i].name; ++i) {
        if (!strcmp(apps[i].name, name)) {
            uart_puts("starting app: ");
            uart_puts(name);
            uart_puts("\n");
            apps[i].fn();
            uart_puts("app finished: ");
            uart_puts(name);
            uart_puts("\n");
            return 0;
        }
    }
    return -1;
}

void app_list(void) {
    uart_puts("apps:\n");
    for (int i = 0; apps[i].name; ++i) {
        uart_puts(" - ");
        uart_puts(apps[i].name);
        uart_puts("\n");
    }
}

/* spawn an app as a new cooperative thread. Returns tid or -1. */
int app_spawn(const char *name) {
    for (int i = 0; apps[i].name; ++i) {
        if (!strcmp(apps[i].name, name)) {
            /* spawn thread; wrapper matches thread_fn(void*) */
            /* cast to thread_fn taking void*; arg unused */
            tid_t tid = thread_spawn((thread_fn)apps[i].fn, NULL, name);
            if (tid < 0) return -1;
            uart_puts("spawned ");
            uart_puts(name);
            uart_puts(" tid:");
            /* print tid */
            char buf[16]; int n=0;
            int v = tid;
            char digits[8]; int d=0;
            if (v==0) digits[d++]='0';
            while (v) { digits[d++] = '0' + (v%10); v/=10; }
            for (int k=d-1;k>=0;--k) buf[n++]=digits[k];
            buf[n]=0;
            uart_puts(buf);
            uart_puts("\n");
            return (int)tid;
        }
    }
    return -1;
}
