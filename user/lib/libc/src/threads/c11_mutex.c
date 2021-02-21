#include <threads.h>
#include <string.h>
#include <stdio.h>

/**
 * Creates mutexes.
 *
 * Currently, only plain mutexes are supported.
 */
int mtx_init(mtx_t *mutex, int type) {
    // XXX: implement other types
    if(type != mtx_plain) {
        fprintf(stderr, "unsupported mutex type %08x for %p", type, mutex);
        return thrd_error;
    }

    // clear the buffer
    memset(&mutex, 0, sizeof(mtx_t));

    return thrd_success;
}

/**
 * Destroys the given mutex.
 */
void mtx_destroy(mtx_t *mutex) {
    // nothing
}

/**
 * Lock a mutex.
 */
int mtx_lock(mtx_t *mutex) {
    while(__sync_lock_test_and_set(&mutex->flag, 1)) {
        thrd_yield();
    }

    return thrd_success;
}

/**
 * Attempts to get a lock, but do not wait.
 */
int mtx_trylock(mtx_t *mutex) {
    if(__sync_lock_test_and_set(&mutex->flag, 1)) {
        return thrd_busy;
    }

    return thrd_success;
}

/**
 * Unlocks a previously locked mutex.
 */
int mtx_unlock(mtx_t *mutex) {
    __sync_lock_release(&mutex->flag);
    return thrd_success;
}

/**
 * If the flag provided is at its initial value (zero) execute the function and set it to one.
 */
void call_once(once_flag * _Nonnull flag, void (* _Nonnull func)(void)) {
    once_flag initial = ONCE_FLAG_INIT;
    once_flag executed = 'A';

    if(__atomic_compare_exchange(flag, &initial, &executed, 0, __ATOMIC_ACQUIRE,
                __ATOMIC_RELAXED)) {
        func();
    }
}
