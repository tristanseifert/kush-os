#include <threads.h>
#include <string.h>
#include <stdio.h>

#include <sys/syscalls.h>

/**
 * Initializes a condition variable.
 */
int cnd_init(cnd_t *cond) {
    fprintf(stderr, "%s unimplemented\n", __FUNCTION__);

    memset(cond, 0, sizeof(cnd_t));
    return thrd_success;
}

/**
 * Releases condition variable resources.
 */
void cnd_destroy(cnd_t *cond) {
    fprintf(stderr, "%s unimplemented\n", __FUNCTION__);
    // TODO: implements
}

/**
 * Wakes up one of the threads waiting on us.
 */
int cnd_signal(cnd_t *cond) {
    fprintf(stderr, "%s unimplemented\n", __FUNCTION__);
    // TODO: implement
    return thrd_error;
}

/**
 * Unblocks all threads waiting on us.
 */
int cnd_broadcast(cnd_t *cond) {
    fprintf(stderr, "%s unimplemented\n", __FUNCTION__);
    return thrd_error;
}

/**
 * Waits for a condition variable to be signalled.
 *
 * We unlock the mutex and block on the condition variable; when we become unblocked again, the
 * mutex will be locked before we return.
 */
int cnd_wait(cnd_t *cond, mtx_t *mtx) {
    fprintf(stderr, "%s unimplemented\n", __FUNCTION__);
    return thrd_error;
}

/**
 * Same as cnd_wait, but with the addition of a timeout.
 */
int cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *ts) {
    fprintf(stderr, "%s unimplemented\n", __FUNCTION__);
    return thrd_error;
}
