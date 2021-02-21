#ifndef LIBC_THREAD_THREAD_INFO_H
#define LIBC_THREAD_THREAD_INFO_H

#include <stddef.h>
#include <stdint.h>

/**
 * State for the userspace threads code
 */
typedef struct {
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
    uintptr_t detached;
    /// when set, thread is executing
    uintptr_t isRunning;

    /// number of threads joined; all but the first must take an extra ref
    size_t numJoining;

    /// if the stack was allocated by us, a pointer to the allocation
    void *stack;
    /// stack size, if known
    size_t stackSize;

    /// return value of the user function that was invoked; provided via thrd_join()
    int exitCode;

    /// auxiliary thread information that needs to be released when we're going away
    void *auxInfo;
} uthread_t;

void __libc_thread_init();

uthread_t *GetThreadInfoBlock(const uintptr_t handle);
uthread_t *CreateThreadInfoBlock(const uintptr_t handle);

/// frees the memory associated with a thread information block unconditionally
void __TIBFree(uthread_t *);

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
