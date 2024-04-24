#ifndef RR_SCHED

#define RR_SCHED

#include <stdlib.h>
#include <stdint.h>

#include "lwp.h"

#define RR_INITIAL_CAP 16
#define RR_REALLOC_FACTOR 2

struct __rr_globals_st {
    // the length of the threads array
    uint64_t len;
    // the capacity of the threads array
    uint64_t cap;
    // the current iteration index
    uint64_t i;
    // the threads array
    thread* threads;
};

static struct __rr_globals_st __rr_globals = {.len= 0, .cap = 0, .threads = NULL};

void rr_shutdown(void) {
    if (__rr_globals.threads == NULL) {
        return;
    }
    free(__rr_globals.threads);
    __rr_globals.threads = NULL;
}

void rr_init(void) {
    // TODO: ensure not already initialized?
    __rr_globals.threads = (thread*)malloc(RR_INITIAL_CAP * sizeof(thread));
    if (__rr_globals.threads == NULL) {
        // TODO: handle error
        return;
    }
    __rr_globals.cap = RR_INITIAL_CAP;
    __rr_globals.len = 0;
}

void rr_admit(thread new) {

    if (__rr_globals.threads == NULL) {
        rr_init();
    }
    // realloc if len == cap
    if (__rr_globals.len >= __rr_globals.cap) {
        __rr_globals.cap *= RR_REALLOC_FACTOR;
        __rr_globals.threads = (thread*)realloc(__rr_globals.threads, __rr_globals.cap * sizeof(thread));
        if (__rr_globals.threads == NULL) {
            // TODO: handle error
            return;
        }
    }
    __rr_globals.threads[__rr_globals.len] = new;
    __rr_globals.len++;
}

void _rr_remove_at(uint64_t i) {
    uint64_t j;

    // copy the threads over one by one... overwriting the one at index i
    // NOTE: no free or anything, we don't assume we own the thread memory
    for (j = i; j < __rr_globals.len - 1; j++) {
        __rr_globals.threads[j] = __rr_globals.threads[j + 1];
    }
    // decrement the current iteration index if it is
    // greater than the index of the removed thread
    // so it stays pointed to the same next thread
    if (i < __rr_globals.i) {
        __rr_globals.i--;
    }
    // decrement the length of the threads array
    __rr_globals.len--;
}

void rr_remove(thread victim) {
    if (__rr_globals.threads == NULL) {
        // TODO: handle error
        return;
    }

    uint64_t i;

    for (i = 0; i < __rr_globals.len; i++) {
        if (__rr_globals.threads[i]->tid != victim->tid) {
            continue;
        }
        _rr_remove_at(i);
        break;
    }

}

thread rr_next(void) {
    if (__rr_globals.threads == NULL || __rr_globals.len == 0) {
        return NULL;
    }

    thread t = NULL;
    uint64_t i = __rr_globals.i;


    for (; i < __rr_globals.len; i++) {
        t = __rr_globals.threads[i];
        if (t == NULL) {
            // TODO: _rr_remove_at(t)?
            continue;
        }
        if (LWPTERMINATED(t->status)) {
            // TODO: _rr_remove_at(t)?
            continue;
        }
        goto postamble;
    }
    for (i = 0; i < __rr_globals.i; i++) {
        t = __rr_globals.threads[i];
        if (t == NULL) {
            // TODO: _rr_remove_at(t)?
            continue;
        }
        if (LWPTERMINATED(t->status)) {
            // TODO: _rr_remove_at(t)?
             continue;
        }
        goto postamble;
    }

    // BASE CASE: if the prev two for loops found an alive thread they would
    // have jumped to the postamble, so if we reach this point we know that
    // no alive threads were found
    t = NULL;

postamble:
    __rr_globals.i = i;
    return t;
}

int rr_qlen(void) {
    // TODO: cache? will we be told when a thread is terminated?
    uint64_t i;
    int qlen = 0;

    for (i = 0; i < __rr_globals.len; i++) {
        if (__rr_globals.threads[i] == NULL) {
            continue;
        }
        if (LWPTERMINATED(__rr_globals.threads[i]->status)) {
            continue;
        }
        qlen++;
    }
    return qlen;
}

struct scheduler_st rr_scheduler = {
    .init = rr_init,
    .shutdown = rr_shutdown,
    .admit = rr_admit,
    .remove = rr_remove,
    .next = rr_next,
    .qlen = rr_qlen,
};

#endif
