#ifndef LIBC_THREAD_THREAD_INFO_H
#define LIBC_THREAD_THREAD_INFO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <threads.h>

/**
 * State for the userspace threads code
 */
typedef struct {
    /// lock over the hashmap
    mtx_t blocksLock;
    /// hashmap containing thread info blocks
    struct hashmap *blocks;
} uthread_state_t;

/**
 * Userspace thread information structure
 */
typedef struct uthread {
    /**
     * Refernce count of the structure.
     *
     * Each thread that's joining with this one will increment this. The thread executing itself
     * also holds a reference. This means that the thread structure is deallocated when all of
     * its references have been released.
     *
     * This ensures that all waiting threads can read the status information out.
     */
    size_t refCount;
    /// native handle of thread
    uintptr_t handle;

    /// when set, the thread is detached
    bool detached;
    /// when set, thread is executing
    bool isRunning;
    /**
     * When set, the thread has been launched and the info block can be considered valid, but it
     * has not yet started executing.
     *
     * This allows us to detect that the thread hasn't gotten its first CPU cycles yet, so we do
     * not assume it's already exited then.
     */
    bool isLaunching;

    /// number of threads joined; all but the first must take an extra ref
    size_t numJoining;

    /// if the stack was allocated by us, a pointer to the allocation
    void *stack;
    /// stack size, if known
    size_t stackSize;

    /// return value of the user function that was invoked; provided via thrd_join()
    int exitCode;

    /// info for thread-local storage
    union {
        /// static thread local info
        struct {
            /// Base of this thread's TLS allocation
            void *base;
            /// total size of the TLS allocation
            size_t length;
            /// how much of the allocated region is for thread-locals?
            size_t tlsRegionLength;
        } s;

        /// dynamic thread local info
        struct {

        } d;
    } tls;

    /// auxiliary thread information that needs to be released when we're going away
    void *auxInfo;
} uthread_t;

LIBC_INTERNAL void __libc_thread_init();

LIBC_INTERNAL uthread_t *GetThreadInfoBlock(const uintptr_t handle);
LIBC_INTERNAL uthread_t *CreateThreadInfoBlock(const uintptr_t handle);

/// frees the memory associated with a thread information block unconditionally
LIBC_INTERNAL void __TIBFree(uthread_t *);

/**
 * Atomically increments the reference count of the given thread.
 */
static inline __attribute__((always_inline)) uthread_t *TIBRetain(uthread_t *thread) {
    __atomic_add_fetch(&thread->refCount, 1, __ATOMIC_ACQUIRE);
    return thread;
}

/**
 * Decrements the thread structure's ref count by one, releasing it if appropriate.
 */
static inline __attribute__((always_inline)) uthread_t *TIBRelease(uthread_t *thread) {
    // refcount == 0?
    if(!__atomic_sub_fetch(&thread->refCount, 1, __ATOMIC_RELEASE)) {
        // release it
        __TIBFree(thread);
        return NULL;
    }

    // refcount >= 1
    return thread;
}

#endif
