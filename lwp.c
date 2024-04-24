#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>

#include "lwp.h"
#include "rr.c"

#define MB (1 << 20)
#define DEFAULT_STACK_SIZE (8 * MB)

#define MAX_THREADS (UINT64_MAX - 1)
/* arbitrary number of threads to allocate space for initially */
#define DEFAULT_NUM_THREADS 32

static struct scheduler_st lwp_current_scheduler;
static scheduler lwp_current_scheduler_ptr = NULL;
static thread_context *lwp_threads = NULL;
static uint64_t lwp_num_threads = 0;
static tid_t lwp_cur_tid = NO_THREAD;
#define dbg(...) fprintf(stderr, __VA_ARGS__)

rlim_t get_stack_size() {
    struct rlimit rlim;

    if (getrlimit(RLIMIT_STACK, &rlim) == -1) {
        return DEFAULT_STACK_SIZE;
    }
    rlim_t stack_limit = rlim.rlim_cur;
    if (stack_limit == RLIM_INFINITY) {
        return DEFAULT_STACK_SIZE;
    }
    assert(stack_limit > 0);
    // ensure stack_limit is a multiple of sizeof(stack)
    assert((stack_limit % sizeof(stack)) == 0);
    return stack_limit;
}

stack *stack_new() {
    uint32_t stack_size = get_stack_size();
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK;
    // let mmap choose fd
    int fd = -1;
    off_t offset = 0;
    stack *stack = mmap(NULL, stack_size, prot, flags, fd, offset);
    return stack;
}

void stack_free(stack *stack) {
    size_t stack_size = get_stack_size();
    munmap(stack, stack_size);
}

void thread_mark_unused(thread t) {
    if (t == NULL) {
        return;
    }
    // set tid to NO_THREAD to indicate the thread has not been initialized
    t->tid = NO_THREAD;
}

// TODO: use flag in thread->status instead of thread->tid
bool thread_is_unused(thread t) {
    if (t == NULL) {
        return false;
    }
    return t->tid == NO_THREAD;
}

// TODO: use flag in thread->status instead of thread->tid
void thread_mark_used(thread t, tid_t tid) {
    if (t == NULL) {
        return;
    }
    t->tid = tid;
}

/**
* Helper function to find an empty thread (as signified by `tid = NO_THREAD`)
* in the thread list
 */
thread thread_list_find_empty() {
    int i;
    // i = 1 to skip the first `NO_THREAD` tid
    for (i = 1; i < lwp_num_threads; i++) {
        thread t = &lwp_threads[i];
        if (thread_is_unused(t)) {
            // found an empty thread
            thread_mark_used(t, i);
            return t;
        }
    }
    return NULL;
}

bool thread_list_has_empty() {
    thread empty = thread_list_find_empty();
    if (empty == NULL) {
        return false;
    }
    // assumes find empty marks the thread as used
    // ensure it isn't marked as used we just want to know
    // if there are unused threads
    thread_mark_unused(empty);
    return true;
}

/**
 * Ensures the thread list has capacity for additional threads
 * If the thread list is not initialized, it is initialized with
 * `DEFAULT_NUM_THREADS` Otherwise thre thread list is reallocated with double
 * the capacity or `MAX_THREADS` whichever is smaller. If the thread list is
 * already at capacity, no action is taken.
 */
void thread_list_ensure_empty_cap() {
    int i;
    if (lwp_threads == NULL) {
        lwp_threads =
            (thread_context *)malloc(DEFAULT_NUM_THREADS * sizeof(thread_context));
        for (i = 0; i < DEFAULT_NUM_THREADS; i++) {
            thread_mark_unused(&lwp_threads[i]);
        }
        lwp_num_threads = DEFAULT_NUM_THREADS;
        return;
    }
    if (thread_list_has_empty()) {
        // if empty thread found, return, as their is capacity for new threads
        return;
    }
    if (lwp_num_threads == MAX_THREADS) {
        // if new_cap is greater than MAX_THREADS, do nothing
        // (trying to initialize a new thread will fail)
        return;
    }

    // if no empty thread found, reallocate
    uint64_t new_cap = lwp_num_threads * 2;
    if (new_cap >= MAX_THREADS) {
        new_cap = MAX_THREADS;
    }
    thread_context* tmp = (thread_context *)realloc(lwp_threads, new_cap * sizeof(thread_context));
    if (tmp == NULL) {
        free(lwp_threads);
        fprintf(stderr, "Failed to allocate more space for new threads");
        exit(1);
    }
    lwp_threads = tmp;
    lwp_num_threads = new_cap;
    return;
}

thread thread_new() {
    thread_list_ensure_empty_cap();
    if (lwp_threads == NULL) {
        return NULL;
    }
    thread empty_thread = thread_list_find_empty();
    if (empty_thread == NULL) {
        return NULL;
    }
    thread_mark_used(empty_thread, empty_thread->tid);
    assert(!thread_is_unused(empty_thread));
    return empty_thread;
}

void thread_init_ctx_no_stack(thread t) {
    t->stacksize = get_stack_size();
    t->status = LWP_LIVE;
    t->lib_one = NULL;
    t->lib_two = NULL;
    t->sched_one = NULL;
    t->sched_two = NULL;
    t->exited = NULL;
    memset(&t->state, 0, sizeof(rfile));
    t->state.fxsave = FPU_INIT;
}

void thread_init_ctx(thread t) {
    thread_init_ctx_no_stack(t);
    t->stack = stack_new();
}

void thread_mark_terminated(thread t, thread_status_t status) {
    if (t == NULL) {
        return;
    }
    t->status = MKTERMSTAT(LWP_TERM, status);
}

/**
 * wraps calling the lwpfun so that if they return an exit status without
 * calling lwp_exit, we call lwp_exit for them with the returned status.
 * This is required because the the return value of the lwpfun will be in %rax
 * while lwp_exit expects the return value to be in %rdi.
 */
static void lwp_wrap(lwpfun fun, void *arg) {
    int return_value;
    return_value = fun(arg);
    lwp_exit(return_value);
}

/**
 * Return a pointer to the next 16 byte aligned entry in the stack
 */
stack* next_16b_aligned_ptr(const stack* s) {
    unsigned long s_ul = (unsigned long)s;
    unsigned long aligned_s = s_ul + (16 - (s_ul % 16));
    assert(aligned_s % 16 == 0);
    assert(sizeof(unsigned long) == sizeof(stack*));
    return (stack*)(aligned_s);
}

void stack_frame_set_old_bp(stack* s, void* bp) {
    s[0] = (unsigned long)bp;
}
void stack_frame_set_ret_addr(stack* s, void* addr) {
    s[1] = (unsigned long)addr;
}

void thread_init_shim_rfile(thread t, lwpfun fun, void* arg) {
    /*
     * End of function prologue:
     * `leave`;
     * `ret`;
     *
     * leave really means:
     * movq %rbp, %rsp ; copy base pointer to stack pointer
     * popq %rbp ; pop the stack into the base pointer
     *
     * and ret means pop the instruction pointer, so the whole thing becomes:
     * movq %rbp, %rsp ; copy base pointer to stack pointer
     * popq %rbp ; pop the stack into the base pointer
     * popq %rip ; pop the stack into the instruction pointer
    */
    if (t == NULL) {
        return;
    }
    stack* lwp_wrap_stack_base = next_16b_aligned_ptr(t->stack);

    // the pointer to the stack frame of the dummy function `swap_rfiles` will
    // tear down with room for the two values that `leave` will pop off the stack
    stack* dummy_frame_stack_base = next_16b_aligned_ptr(&lwp_wrap_stack_base[1]);

    // movq %rbp, %rsp ; copy base pointer to stack pointer
    // in order for stack pointer to be setup by swap_rfiles, %rbp must be set
    // to the base of the stack
    t->state.rbp = (unsigned long)dummy_frame_stack_base;

    // popq %rbp ; pop the stack into the base pointer
    // set the address of the stack frame that will be returned to by `leave`
    // to be the base of the lwpfun stack frame
    dummy_frame_stack_base[0] = (unsigned long)lwp_wrap_stack_base;
    // popq %rip ; pop the stack into the instruction pointer
    // set the ip popped off the stack to the address of `lwp_wrap`
    dummy_frame_stack_base[-1] = (unsigned long)lwp_wrap;

    // set the first argument to lwp_wrap to be lwpfun
    t->state.rdi = (unsigned long)fun;
    // set the second argument to lwp_wrap to be arg
    t->state.rsi = (unsigned long)arg;
}


tid_t lwp_create(lwpfun fun, void *arg) {
    thread t = thread_new();
    if (t == NULL) {
        fprintf(stderr, "lwp_create: failed to create new thread\n");
        return NO_THREAD;
    }
    thread_init_ctx(t);
    thread_init_shim_rfile(t, fun, arg);
    scheduler s = lwp_get_scheduler();
    if (s == NULL) {
        fprintf(stderr, "lwp_create: scheduler is NULL\n");
        return NO_THREAD;
    }
    s->admit(t);
    return t->tid;
}

void lwp_start(void) {
    thread_list_ensure_empty_cap();
    // use the reserved thread 0 (`NO_THREAD`) as the main thread
    thread t = thread_list_find_empty();
    // init the threads context but do not allocate a stack (use current stack instead)
    thread_init_ctx_no_stack(t);
    t->stack = NULL;
    scheduler s = lwp_get_scheduler();
    if (s == NULL) {
        fprintf(stderr, "lwp_start: scheduler is NULL\n");
        return;
    }
    s->admit(t);
    // save current register values in t->state
    swap_rfiles(&t->state, NULL);
    lwp_cur_tid = t->tid;
    lwp_yield();
}

int thread_get_status(thread t) {
    if (t == NULL) {
        return LWP_TERM;
    }
    return LWPTERMSTAT(t->status);
}

void lwp_yield(void) {
    scheduler s = lwp_get_scheduler();
    thread next = s->next();
    tid_t cur_tid = lwp_gettid();
    thread cur = tid2thread(cur_tid);
    if (next == NULL) {
        int status = thread_get_status(cur);
        exit(status);
    }
    if (cur == NULL) {
        // NOTE: could use tid=0 as junk area to call swap_rfiles on
        fprintf(stderr, "lwp_yield: cur is NULL\n");
        exit(1);
    }
    if (cur->tid == next->tid) {
        fprintf(stderr, "lwp_yield: next is cur\n");
        exit(1);
    }
    dbg("lwp_yield: switching from %lu to %lu\n", cur->tid, next->tid);
    lwp_cur_tid = next->tid;
    // save all current registers values to cur->state
    // and load all register values from next->state
    // NOTE: must be last call as execution will continue where it left off
    // when yielding to a thread that previously yielded
    swap_rfiles(&cur->state, &next->state);
}

void lwp_exit(thread_status_t status) {
    tid_t cur_tid = lwp_gettid();
    thread cur = tid2thread(cur_tid);

    if (cur == NULL) {
        fprintf(stderr, "lwp_exit: cur is NULL... yielding\n");
        lwp_yield();
        return;
    }
    thread_mark_terminated(cur, status);

    lwp_yield();
}

tid_t lwp_wait(int *status) {
    if (status != NULL) {
        *status = 0;
    }
    return NO_THREAD;
}

tid_t lwp_gettid(void) {
    return lwp_cur_tid;
}

thread tid2thread(tid_t tid) {
    if (lwp_threads == NULL) {
        return NULL;
    }
    if (tid == NO_THREAD) {
        return NULL;
    }
    return &lwp_threads[tid];
}

struct scheduler_st default_scheduler(void) {
    return rr_scheduler;
}

void lwp_set_scheduler(scheduler s) {
    scheduler cur = lwp_current_scheduler_ptr;
    if (cur != NULL && cur->shutdown != NULL) {
        cur->shutdown();
    }
    if (s == NULL) {
        lwp_current_scheduler = default_scheduler();
        lwp_current_scheduler_ptr = &lwp_current_scheduler;
        return;
    }
    if (s->init != NULL) {
        s->init();
    }
    // copy in s so it can't be changed out from under us, only with `lwp_set_scheduler`
    lwp_current_scheduler = *s;
    lwp_current_scheduler_ptr = &lwp_current_scheduler;
}

scheduler lwp_get_scheduler(void) {
    if (lwp_current_scheduler_ptr == NULL) {
        // set scheduler to default if not already set before returning it
      lwp_set_scheduler(NULL);
      assert(lwp_current_scheduler_ptr != NULL);
    }
    return lwp_current_scheduler_ptr;
}
