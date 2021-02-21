#include "thread_info.h"

#include <assert.h>
#include <malloc.h>
#include <stdlib.h>
#include <time.h>

#include <sys/syscalls.h>
#include <sys/time.h>
#include <threads.h>

/// Threshold for sleep below which we'll busy wait rather than going to the kernel, in ns
#define kBusyWaitThreshold (1000 * 1000)

/**
 * Stack size for new threads, in units of words (uintptr_t). When zero, an implementation-
 * defined default value will be used.
 *
 * This can be changed with the non-portable call thrd_set_stacksize_np().
 */
static size_t gThreadStackSz = 0;

#if defined(__i386__)
// 128K
#define kDefaultStackSz                 ((128 * 1024) / sizeof(uintptr_t))
#else
#error Update threads/c11_threads.c with this arch's default stack size
#endif

/**
 * Information structure to pass to newly created threads
 */
struct NewThreadInfo {
    // entry point and its associated context
    thrd_start_t entry;
    void *entryCtx;

    // pointer to the thread info block for this thread
    thrd_t thrd;
};


/**
 * Thread entry trampoline; all threads created by means of the C library will jump through this
 * call, which ensures stuff like thread-locals are set up correctly.
 */
static void __ThreadEntry(uintptr_t arg) {
    int err;

    // get the info structure
    struct NewThreadInfo *info = (struct NewThreadInfo *) arg;
    assert(info);

    // extract the entry point and release the info structure
    uintptr_t yes = 1;
    __atomic_store(&info->thrd->isRunning, &yes, __ATOMIC_RELEASE);

    // TODO: more initialization

    // invoke the user's function
    err = info->entry(info->entryCtx);

    // TODO: cleanup

    // terminate in the usual way
    thrd_exit(err);
}


/**
 * Alocates a new thread.
 *
 * This will also set up the thread's stack, with an implementation-defined default stack size.
 */
int thrd_create(thrd_t *outThread, thrd_start_t entry, void *arg) {
    int err;
    uintptr_t handle;

    // allocate the info structure to pass to the thread entry point
    struct NewThreadInfo *info = (struct NewThreadInfo *) calloc(1, sizeof(struct NewThreadInfo));
    if(!info) {
        return thrd_nomem;
    }

    info->entry = entry;
    info->entryCtx = arg;

    // set up a stack
    const size_t stackBytes = (gThreadStackSz ? gThreadStackSz : kDefaultStackSz) * sizeof(uintptr_t);
    void *stack = valloc(stackBytes);

    if(!stack) {
        free(info);
        return thrd_nomem;
    }

    // create the thread, but paused
    uintptr_t stackBot = ((uintptr_t) stack) + stackBytes;

    err = ThreadCreateFlags(__ThreadEntry, (uintptr_t) info, stackBot, &handle, THREAD_PAUSED);
    if(err) {
        free(info);
        free(stack);
        return thrd_error;
    }

    // set up an info block
    uthread_t *thread = CreateThreadInfoBlock(handle);

    thread->auxInfo = info;
    thread->stack = stack;
    thread->stackSize = stackBytes;

    // +1 = 2 refs now; must either join or detach thread
    TIBRetain(thread);
    info->thrd = thread;

    // resume the thread and write its identifier out
    *outThread = thread;

    err = ThreadResume(handle);
    if(err) {
        // XXX: is there anything we can do here? the thread might run at some point later...
    }

    return thrd_success;
}

/**
 * Returns the thread info for the current thread.
 *
 * If we don't already have a thread info block (for example, for the main thread created at
 * program start, or threads created via the native syscalls) we'll create one.
 */
thrd_t thrd_current() {
    int err;
    uintptr_t handle;

    // retrieve the current thread's handle
    err = ThreadGetHandle(&handle);
    assert(!err);

    // look up thread info, and return its location if found
    uthread_t *thread = GetThreadInfoBlock(handle);
    if(thread) {
        return thread;
    }

    // create a thread block for the current thread and return it
    return CreateThreadInfoBlock(handle);
}

/**
 * Compares two thread objects to see if they're equal. This compares the underlying handles.
 */
int thrd_equal(thrd_t thr0, thrd_t thr1) {
    return (thr0->handle == thr1->handle) ? 1 : 0;
}

/**
 * Terminates the calling thread.
 */
void thrd_exit(int res) {
    int err;
    uintptr_t handle;

    // get the handle
    uthread_t *thread = thrd_current();
    handle = thread->handle;

    // store exit code and clear the running flag
    thread->exitCode = res;

    uintptr_t no = 0;
    __atomic_store(&thread->isRunning, &no, __ATOMIC_RELEASE);

    // release TIB ref
    TIBRelease(thread);

    // TODO: get rid of the stack

    // terminate the thread
    err = ThreadDestroy(handle);
    assert(!err);

    // REALLY shouldn't get here
    abort();
}

/**
 * Sleeps the thread for a certain amount of time.
 *
 * Our sleep syscall has only microsecond resolution, so for any sleeps less than about 2mS, we
 * simply busy wait. Anything above that takes a trip into the kernel, and it's very likely you'll
 * sleep for much longer (or shorter) than requested.
 *
 * TODO: Write the result to the remaining pointer
 *
 * @return 0 if sleep succeeded, -1 if it was interrupted, any other negative value on error
 */
int thrd_sleep(const struct timespec *duration, struct timespec *remaining) {
    int err = 0;
    struct timeval start, end;

    // get the time we started if caller cares about it
    if(remaining) {
        gettimeofday(&start, NULL);
    }

    // for short waits, busy wait
    if(!duration->tv_sec && duration->tv_nsec < kBusyWaitThreshold) {
        // TODO: busy wait
        goto done;
    }
    // otherwise, take a trip to the kernel
    else {
        const uint64_t time = (duration->tv_nsec) + (duration->tv_sec * (1000000000ULL));
        const uintptr_t usecs = (time / 1000ULL);

        err = ThreadUsleep(usecs);

        if(!err) {
            goto done;
        } 
        // if error, assume we got interrupted
        else {
            err = -1;
            goto done;
        }
    }

done:;
    // calculate the actual time taken
    if(remaining) {
        gettimeofday(&end, NULL);

        const uint64_t micros = ((end.tv_sec * 1000000ULL + end.tv_usec) - 
                (start.tv_sec * 1000000ULL + start.tv_usec));

        remaining->tv_nsec = (micros % 1000000ULL) * 1000;
        remaining->tv_sec = (micros / 1000000ULL);
    }

    return err;
}

/**
 * Gives up the remainder of the caller's processor time slice.
 */
void thrd_yield(void) {
    ThreadYield();
}

/**
 * Waits for the given thread to exit for the given amount of time.
 *
 * @note On 32-bit platforms, the duration to wait is at most 2^32 - 2 microseconds, or about one
 * hour and 11 minutes. If the wait duration is higher than this, we'll cap it at this.
 */
int thrd_join_np(thrd_t _thread, int *outRes, const struct timespec *howLong) {
    int err;
    uintptr_t wait = UINTPTR_MAX;

    // ensure we haven't been detached
    uintptr_t detached;
    __atomic_load(&_thread->detached, &detached, __ATOMIC_CONSUME);
    if(detached) {
        return thrd_error;
    }

    // calculate the wait time
    if(howLong) {
        // if the duration is 0, poll for termination
        if(!howLong->tv_sec && !howLong->tv_nsec) {
            wait = 0;
        }
        // otherwise, convert to microseconds
        else {
            /*
             * Explicitly do the math in 64 bit; this makes detecting overflowing the 32 bit
             * value easier for such platforms.
             */
            const uint64_t micros = (howLong->tv_nsec / 1000ULL) + (howLong->tv_sec * 1000000ULL);

#if defined(__i386__)
            if(micros >= (UINTPTR_MAX - 1)) {
                wait = (UINTPTR_MAX - 1);
            } else {
                wait = micros;
            }
#else
            wait = micros;
#endif
        }
    }

    // handle the instance in which the thread has already exited
    uintptr_t isRunning;
    __atomic_load(&_thread->isRunning, &isRunning, __ATOMIC_CONSUME);

    if(!isRunning) {
        if(outRes) {
            *outRes = _thread->exitCode;
        }

        TIBRelease(_thread);
        return thrd_success;
    }

    // register our wait interest and block on the thread
    thrd_t thread;

    if(__atomic_fetch_add(&_thread->numJoining, 1, __ATOMIC_RELAXED)) {
        thread = TIBRetain(_thread);
    } else {
        thread = _thread;
    }

    err = ThreadWait(thread->handle, wait);
    if(err < 0) {
        // error while waiting
        TIBRelease(thread);
        return thrd_error;
    } else if(err == 1) {
        // timeout expired
        TIBRelease(thread);
        return thrd_timedout;
    }

    // read out the return value
    if(outRes) {
        *outRes = thread->exitCode;
    }

    // release our reference on the thread
    TIBRelease(thread);
    return thrd_success;
}

/**
 * Wraps the above non-portable thread join routine. This just makes sure that the timeout error
 * (which we really should never get...) gets converted to a generic error.
 */
int thrd_join(thrd_t thread, int *outRes) {
    int err = thrd_join_np(thread, outRes, NULL);

    if(err == thrd_timedout) {
        return thrd_error;
    }

    return err;
}

/**
 * Detaches the thread.
 *
 * This means that the thread will release all of its date (including the TIB) when it terminates,
 * rather than requiring an explcit later call to thrd_join() to do so.
 *
 * We implement this by simply taking away one reference from the thread. Since we return an object
 * from thrd_create() with two references, this ensures it destroys all data when the thread
 * exits. Likewise, if the thread has already exited, there will still be one reference to the
 * thread object, and this will destroy it.
 */
int thrd_detach(thrd_t thread) {
    // ensure this thread hasn't been detached before
    if(!thread) {
        return thrd_error;
    }

    uintptr_t no = 0, yes = 1;

    if(!__atomic_compare_exchange(&thread->detached, &no, &yes, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
        // already detached!
        return thrd_error;
    }

    // take away the reference
    TIBRelease(thread);

    return thrd_success;
}


/**
 * Sets the stored stack size.
 */
int thrd_set_stacksize_np(const size_t size) {
    gThreadStackSz = size;
    return thrd_success;
}

/**
 * Return the native handle for the thread.
 */
uintptr_t thrd_get_handle_np(thrd_t thread) {
    return thread->handle;
}
