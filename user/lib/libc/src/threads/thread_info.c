#include "thread_info.h"
#include "struct/hashmap.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <sys/syscalls.h>

static uthread_state_t gState;

/**
 * Returns a hash code for a thread struct.
 *
 * This simply hashes the thread's handle.
 */
static uint64_t _HashmapThreadHash(const void *item, uint64_t seed0, uint64_t seed1) {
    const uthread_t *thread = (const uthread_t *) item;
    return hashmap_sip(&thread->handle, sizeof(thread->handle), seed0, seed1);
}
/**
 * Compares two threads by their handles.
 */
static int _HashmapThreadCompare(const void *a, const void *b, void *udata) {
    const uthread_t *ta = (const uthread_t *) a;
    const uthread_t *tb = (const uthread_t *) b;

    return ta->handle < tb->handle;
}


/**
 * Initializes the userspace threading component.
 */
void __libc_thread_init() {
    int err = mtx_init(&gState.blocksLock, mtx_plain);
    if(err != thrd_success) abort();

    gState.blocks = hashmap_new(sizeof(uthread_t), 0, 0, 0, _HashmapThreadHash,
            _HashmapThreadCompare, NULL);
}

/**
 * Returns the thread info structure for a thread given its handle.
 */
uthread_t *GetThreadInfoBlock(const uintptr_t handle) {
    int err;
    uthread_t query = { .handle = handle  };
    uthread_t *found = NULL;

    // retrieve an existing block if it exists
    err = mtx_lock(&gState.blocksLock);
    if(err != thrd_success) return NULL;

    void *_info = hashmap_get(gState.blocks, &query);
    if(_info) {
        found = (uthread_t *) _info;
        goto beach;
    }

beach:;
    // no info block found
    mtx_unlock(&gState.blocksLock);
    return found;
}

/**
 * Allocates a new thread info block for the given thread. The only information that's filled in
 * is the handle.
 *
 * The returned object will have one reference to it.
 */
uthread_t *CreateThreadInfoBlock(const uintptr_t handle) {
    int err;

    uthread_t thread;
    memset(&thread, 0, sizeof(thread));
    thread.handle = handle;
    thread.refCount = 1;

    err = mtx_lock(&gState.blocksLock);
    if(err != thrd_success) return NULL;

    void *ret = hashmap_set(gState.blocks, &thread);
    assert(ret || (!ret && !hashmap_oom(gState.blocks)));

    mtx_unlock(&gState.blocksLock);

    // then return the freshly inserted object
    return GetThreadInfoBlock(handle);
}

/**
 * Releases all memory associated with a thread information block, and removes it from the our
 * global handle mapping.
 */
void __TIBFree(uthread_t *thread) {
    int err = mtx_lock(&gState.blocksLock);
    if(err != thrd_success) return;

    // free its memory
    if(thread->auxInfo) {
        free(thread->auxInfo);
    }

    // remove from map (releases its memory as well)
    void *old = hashmap_delete(gState.blocks, thread);
    assert(old);

    mtx_unlock(&gState.blocksLock);
}
