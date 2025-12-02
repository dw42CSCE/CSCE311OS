#ifndef THREAD_H
#define THREAD_H

#include <stddef.h>

typedef int tid_t;

/* thread function type */
typedef void (*thread_fn)(void *);

/* create a thread (returns tid, or -1 on failure) */
tid_t thread_spawn(thread_fn fn, void *arg, const char *name);

/* cooperative yield */
void thread_yield(void);

/* sleep for N scheduler ticks (cooperative) */
void thread_sleep(int ticks);

/* scheduler tick (call from main loop to run threads) */
void sched_tick(void);

/* list threads into uart (ps) */
void thread_list(void);

/* kill thread by id (returns 0 on success) */
int thread_kill(tid_t tid);

#endif
