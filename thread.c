#include "thread.h"
#include "uart.h"
#include "string.h"
#include <stddef.h>

/* Cooperative threading: fixed-size table and static stacks. */

#define MAX_THREADS 16
#define STACK_SIZE 4096
void thread_exit(void);

enum {
    THREAD_READY = 0,
    THREAD_RUNNING = 1,
    THREAD_FINISHED = 2,
    THREAD_SLEEPING = 3
};

typedef struct {
    int used;
    tid_t id;
    char name[16];
    unsigned long regs[14]; /* saved context: ra, sp, s0-s11 */
    thread_fn fn;
    void *arg;
    int state; /* THREAD_* */
    int sleep_ticks; /* remaining ticks if sleeping */
} thread_t;

static thread_t threads[MAX_THREADS];
/* per-thread stacks that will be used for SP initialization */
static unsigned char stacks[MAX_THREADS][STACK_SIZE];

static tid_t next_tid = 1;
static int cur = -1; /* current running thread index, -1 = main */

/* persistent storage for the shell/main context so we can restore it */
static unsigned long main_regs[14];
static int main_saved = 0; /* whether main_regs currently holds saved state */

/* context switch implemented in assembly (defined in context.S) */
void context_switch(unsigned long *old_regs, unsigned long *new_regs);

/* trampoline implemented in C (thread_trampoline.c) */
extern void thread_trampoline(void);

void thread_start_run(void) {
    uart_puts("[thread_start_run] enter\n");

    if (cur < 0 || cur >= MAX_THREADS || !threads[cur].used) {
        uart_puts("[thread_start_run] ERROR: no current thread\n");
        /* nothing sensible to do — halt */
        while (1) asm volatile("wfi");
    }

    /* mark running */
    threads[cur].state = THREAD_RUNNING;

    /* call the thread function */
    if (threads[cur].fn) {
        /* thread_fn has signature void(*)(void*), apps cast to that when spawned */
        threads[cur].fn(threads[cur].arg);
    }

    uart_puts("[thread_start_run] thread fn returned\n");
    uart_puts("[thread_start_run] about to clean up / exit\n");
    uart_puts("[thread_start_run] call thread_exit()\n");
    thread_exit();

    /* should never get here */
    uart_puts("[thread_start_run] ERROR: returned from thread_exit\n");
    while (1) asm volatile("wfi");
}

void thread_exit(void) {
    if (cur == -1) {
        uart_puts("[thread_exit] ERROR: thread_exit called with no current thread\n");
        while (1) asm volatile("wfi");
    }

    int prev = cur;
    threads[prev].state = THREAD_FINISHED; /* mark finished */
    uart_puts("[thread_exit] thread marked finished\n");

    /* find next ready thread */
    int next = -1;
    for (int i = 1; i <= MAX_THREADS; ++i) {
        int idx = (prev + i) % MAX_THREADS;
        if (threads[idx].used && threads[idx].state == THREAD_READY) { next = idx; break; }
    }

    if (next != -1) {
        /* switch directly to next ready thread (never returns) */
        uart_puts("[thread_exit] switching to next ready thread\n");
        cur = next;
        context_switch(threads[prev].regs, threads[cur].regs);
        /* never returns */
        while (1) asm volatile("wfi");
    }

    /* no other ready thread: restore main if saved, otherwise spin */
    cur = -1;
    if (main_saved) {
        uart_puts("[thread_exit] restoring main context\n");
        main_saved = 0;
        context_switch(threads[prev].regs, main_regs);
        /* when/if this returns, we're back in main */
        return;
    }

    /* nothing to run and main not saved -> idle forever */
    uart_puts("[thread_exit] no threads left, halting\n");
    while (1) asm volatile("wfi");
}

tid_t thread_spawn(thread_fn fn, void *arg, const char *name) {
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (!threads[i].used) {
            threads[i].used = 1;
            threads[i].id = next_tid++;
            threads[i].fn = fn;
            threads[i].arg = arg;
            threads[i].state = THREAD_READY;
            threads[i].sleep_ticks = 0;
            /* copy name safely */
            int j;
            for (j = 0; j < 15 && name && name[j]; ++j) threads[i].name[j] = name[j];
            threads[i].name[j] = '\0';
            /* clear saved registers */
            for (int r = 0; r < 14; ++r) threads[i].regs[r] = 0;
            /* set ra to trampoline so when context restores it will jump into trampoline */
            threads[i].regs[0] = (unsigned long)thread_trampoline; /* ra */
            /* set sp to top of the thread's dedicated stack */
            threads[i].regs[1] = (unsigned long)&stacks[i][STACK_SIZE];
            return threads[i].id;
        }
    }
    return -1;
}

static int find_idx_by_tid(tid_t tid) {
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (threads[i].used && threads[i].id == tid) return i;
    }
    return -1;
}

/* cooperative yield: switch to next ready thread or return to main if none */
void thread_yield(void) {
    int old = cur;
    int next = -1;
    for (int i = 1; i <= MAX_THREADS; ++i) {
        int idx = (old + i) % MAX_THREADS;
        if (threads[idx].used && threads[idx].state == THREAD_READY) { next = idx; break; }
    }

    /* If there is a next ready thread, do a normal switch (or start it). */
    if (next != -1) {
        if (old == -1) {
            /* yielding from main into a thread */
            cur = next;
            threads[cur].state = THREAD_RUNNING;     /* mark as running */
            main_saved = 1;
            context_switch(main_regs, threads[cur].regs);
            return;
        } else {
            int prev = old;
            /* mark prev as ready, next as running */
            if (threads[prev].state == THREAD_RUNNING) {
                threads[prev].state = THREAD_READY;
            }
            cur = next;
            threads[cur].state = THREAD_RUNNING;
            context_switch(threads[prev].regs, threads[cur].regs);
            return;
        }
    }

    /* No other ready thread found. If we're a thread (old != -1) we should
       restore main (if it was saved). In this case mark the yielding thread
       back to READY so it can be scheduled later. */
    if (old != -1) {
        int prev = old;
        /* mark thread no longer running, back to ready */
        if (threads[prev].state == THREAD_RUNNING) {
            threads[prev].state = THREAD_READY;
        }

        cur = -1;
        if (main_saved) {
            /* restore saved main registers so the shell resumes */
            main_saved = 0;
            context_switch(threads[prev].regs, main_regs);
            /* when this returns, execution is back in main */
            return;
        }
        /* if main wasn't saved, simply return (nothing to switch to) */
    }

    /* old == -1 and no ready threads: nothing to do */
    return;
}

/* scheduler tick: start a thread if none running, or cleanup finished ones */
void sched_tick(void) {
    /* wake sleeping threads and move them back to ready when their timer hits 0 */
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (threads[i].used && threads[i].state == THREAD_SLEEPING) {
            if (threads[i].sleep_ticks > 0) threads[i].sleep_ticks--;
            if (threads[i].sleep_ticks <= 0) threads[i].state = THREAD_READY;
        }
    }

    if (cur == -1) {
        /* pick a ready thread to run, if any */
        for (int i = 0; i < MAX_THREADS; ++i) {
            if (threads[i].used && threads[i].state == THREAD_READY) {
                /* start this thread; if main hasn't been saved, save it */
                cur = i;
		threads[cur].state = THREAD_RUNNING; /* mark as running */
		if (!main_saved) {
		    main_saved = 1;
		    context_switch(main_regs, threads[cur].regs);
		} else {
    		    unsigned long dummy[14] = {0};
		    context_switch(dummy, threads[cur].regs);
		}

                break;
            }
        }
    } else {
        /* if current thread finished, free it and restore main if needed */
        if (threads[cur].state == THREAD_FINISHED) {
            int finished = cur;
            /* clear the thread slot */
            threads[finished].used = 0;
            cur = -1;
            if (main_saved) {
                main_saved = 0;
                /* restore main context using finished thread's regs as old_regs */
                context_switch(threads[finished].regs, main_regs);
                /* when this returns we are back in main */
            }
        }
    }
}

void thread_list(void) {
    uart_puts("threads:\n");
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (threads[i].used) {
            char buf[80]; int n = 0;
            const char *p = " id:";
            while (*p) buf[n++] = *p++;
            /* id */
            int id = threads[i].id;
            char digits[16]; int d = 0;
            if (id == 0) digits[d++] = '0';
            while (id) { digits[d++] = '0' + (id % 10); id /= 10; }
            for (int k = d - 1; k >= 0; --k) buf[n++] = digits[k];
            p = " name:";
            while (*p) buf[n++] = *p++;
            p = threads[i].name;
            while (*p) buf[n++] = *p++;
            p = " state:";
            while (*p) buf[n++] = *p++;
            const char *st = (threads[i].state == THREAD_READY) ? "ready" :
                             (threads[i].state == THREAD_RUNNING) ? "run" :
                             (threads[i].state == THREAD_SLEEPING) ? "sleep" :
                             (threads[i].state == THREAD_FINISHED) ? "fin" : "?";
            while (*st) buf[n++] = *st++;
            if (threads[i].state == THREAD_SLEEPING) {
                p = " ticks:";
                while (*p) buf[n++] = *p++;
                int t = threads[i].sleep_ticks;
                char digits2[16]; int d2 = 0;
                if (t == 0) digits2[d2++] = '0';
                while (t) { digits2[d2++] = '0' + (t % 10); t /= 10; }
                for (int k = d2 - 1; k >= 0; --k) buf[n++] = digits2[k];
            }
            buf[n++] = '\n';
            buf[n] = '\0';
            uart_puts(buf);
        }
    }
}

void thread_sleep(int ticks) {
    if (ticks <= 0) {
        thread_yield();
        return;
    }
    if (cur < 0 || cur >= MAX_THREADS) return;
    threads[cur].state = THREAD_SLEEPING;
    threads[cur].sleep_ticks = ticks;
    thread_yield();
}

int thread_kill(tid_t tid) {
    int idx = find_idx_by_tid(tid);
    if (idx < 0) return -1;
    threads[idx].used = 0;
    if (cur == idx) cur = -1;
    return 0;
}
