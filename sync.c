#include "sync.h"
#include "uart.h"

/* Tiny cooperative mutex/semaphore helpers. Busy-wait with yields. */

void mutex_init(mutex_t *m) {
    if (!m) return;
    m->locked = 0;
    m->owner = 0;
}

int mutex_trylock(mutex_t *m) {
    if (!m) return -1;
    if (m->locked) return -1;
    m->locked = 1;
    m->owner = 0;
    return 0;
}

void mutex_lock(mutex_t *m) {
    if (!m) return;
    while (1) {
        if (!m->locked) {
            m->locked = 1;
            m->owner = 0;
            return;
        }
        thread_yield();
    }
}

void mutex_unlock(mutex_t *m) {
    if (!m) return;
    m->locked = 0;
    m->owner = 0;
}

void sem_init(semaphore_t *s, int initial) {
    if (!s) return;
    s->count = initial;
}

void sem_post(semaphore_t *s) {
    if (!s) return;
    s->count++;
}

void sem_wait(semaphore_t *s) {
    if (!s) return;
    while (s->count <= 0) {
        thread_yield();
    }
    s->count--;
}

void barrier_init(barrier_t *b, int needed) {
    if (!b) return;
    if (needed < 1) needed = 1;
    b->needed = needed;
    b->count = 0;
    b->generation = 0;
}

void barrier_wait(barrier_t *b) {
    if (!b) return;
    int my_gen = b->generation;
    b->count++;
    if (b->count >= b->needed) {
        b->count = 0;
        b->generation++;
        return;
    }
    while (b->generation == my_gen) {
        thread_yield();
    }
}
