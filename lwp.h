// vim:filetype=c
#ifndef LWPH
#define LWPH

#include <stdbool.h>
#include <sys/types.h>

#if defined(_x86_64) || defined(__x86_64__) || defined(__amd64__) ||           \
    defined(__amd64)

#include "fp.h"

struct __attribute__((aligned(16))) __attribute__((packed)) registers {
  /* the sixteen architecturally-visible regs. */
  unsigned long rax;
  unsigned long rbx;
  unsigned long rcx;
  unsigned long rdx;
  unsigned long rsi;
  unsigned long rdi;
  unsigned long rbp;
  unsigned long rsp;
  unsigned long r8;
  unsigned long r9;
  unsigned long r10;
  unsigned long r11;
  unsigned long r12;
  unsigned long r13;
  unsigned long r14;
  unsigned long r15;
  struct fxsave fxsave; /* space to save floating point state */
};
typedef struct registers rfile;
#else
#error "This only works on x86_64 for now"
#endif

typedef unsigned long tid_t;

#define NO_THREAD 0 /* an always invalid thread id */
#define THREAD_CTR_START 1

typedef struct threadinfo_st* thread;

typedef unsigned int thread_status_t;

typedef unsigned long stack;

struct threadinfo_st {
  tid_t tid;              /* lightweight process id */
  stack* stack;           /* Base of allocated stack */
  size_t stacksize;       /* Size of allocated stack */
  rfile state;            /* saved registers */
  thread_status_t status; /* exited? exit status?  */
  thread lib_one;         /* Two pointers reserved */
  thread lib_two;         /* for use by the library */
  thread sched_one;       /* Two more for */
  thread sched_two;       /* schedulers to use */
  thread exited;          /* and one for lwp_wait() */
};
typedef struct threadinfo_st thread_context;
typedef struct threadinfo_st* thread;

typedef int (*lwpfun)(void *); /* type for lwp function */

/* Tuple that describes a scheduler */
struct scheduler_st {
  void (*init)(void);            /* NULLABLE - initialize any structures */
  void (*shutdown)(void);        /* NULLABLE - tear down any structures */
  void (*admit)(thread nt);      /* add a thread to the pool */
  void (*remove)(thread victim); /* remove a thread from the pool */
  thread (*next)(void);          /* select a thread to schedule */
  int (*qlen)(void);             /* number of ready threads */
};
typedef struct scheduler_st* scheduler;

/**
 * Creates a new thread and admits it to the current scheduler. The thread’s
 * resources will consist of a context and stack, both initialized so that when
 * the scheduler chooses this thread and its context is loaded via swap_rfiles()
 * it will run the given function. This may be called by any thread.
 */
extern tid_t lwp_create(lwpfun, void *);

/**
 * Terminates the calling thread. Its termination status becomes the low 8 bits
 * of the passed integer. The thread’s resources will be deallocated once it is
 * waited for in lwp_wait(). Yields control to the next thread using
 * lwp_yield().
 */
extern void lwp_exit(thread_status_t);
/**
 * Returns the tid of the calling thread or NO_THREAD if not called from within
 * a LWP
 */
extern tid_t lwp_gettid(void);
/**
 * Yields control to the next thread as indicated by the scheduler. If there is
 * no next thread, calls exit(3) with the termination status of the calling
 * thread (see below).
 */
extern void lwp_yield(void);
/**
 * Starts the threading system by converting the calling thread—the original
 * system thread—into a LWP by allocating a context for it and admitting it to
 * the scheduler, and yields control to whichever thread the scheduler
 * indicates. It is not necessary to allocate a stack for this thread since it
 * already has one.
 */
extern void lwp_start(void);
// NOTE: unused?
// extern void lwp_stop(void);
/**
 * Deallocates the resources of a terminated LWP. If no LWPs have terminated and
 * there still exist runnable threads, blocks until one terminates. If status is
 * non-NULL, status is populated with its termination status. Returns the tid
 * of the terminated thread or NO_THREAD if it would block forever because there
 * are no more runnable threads that could terminate. Be careful not to
 * deallocate the stack of the thread that was the original system thread.
 */
extern tid_t lwp_wait(int *);
/**
 * Sets the scheduler to the one given,
 * reverting to round robin scheduling if the scheduler is NULL
 */
extern void lwp_set_scheduler(scheduler);
/**
 * Returns the current scheduler
 */
extern scheduler lwp_get_scheduler(void);
/**
 * Returns the thread associated with the given tid, or NULL if the ID is
 * invalid
 */
extern thread tid2thread(tid_t tid);

/* for lwp_wait */
#define TERMOFFSET 8

#define MKTERMSTAT(a, b) ((a) << TERMOFFSET | ((b) & ((1 << TERMOFFSET) - 1)))
#define LWP_TERM 1
#define LWP_LIVE 0
#define LWPTERMINATED(s) ((((s) >> TERMOFFSET) & LWP_TERM) == LWP_TERM)
#define LWPTERMSTAT(s) ((s) & ((1 << TERMOFFSET) - 1))

/* prototypes for asm functions */
void swap_rfiles(rfile *old, rfile *new);

#endif
