#ifndef SYNC_H
#define SYNC_H

#include "thread.h"

typedef struct {
    volatile int locked;
    tid_t owner;
} mutex_t;

void mutex_init(mutex_t *m);
int mutex_trylock(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

typedef struct {
    volatile int count;
} semaphore_t;

void sem_init(semaphore_t *s, int initial);
void sem_post(semaphore_t *s);
void sem_wait(semaphore_t *s);

typedef struct {
    int needed;
    int count;
    int generation;
} barrier_t;

void barrier_init(barrier_t *b, int needed);
void barrier_wait(barrier_t *b);

#endif
