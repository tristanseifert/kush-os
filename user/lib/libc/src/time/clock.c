#include <errno.h>
#include <stdio.h>
#include <time.h>

/**
 * Reads out the current time according to the specified clock id.
 */
int clock_gettime(clockid_t clock_id, struct timespec *tp) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    
    errno = EINVAL;
    return -1;
}

/**
 * Returns the resolution of the given clock.
 */
int clock_getres(clockid_t clock_id, struct timespec *tp) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    
    errno = EINVAL;
    return -1;
}

/**
 * Setting clocks is not supported.
 */
int clock_settime(clockid_t clock_id, const struct timespec *tp) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    
    errno = EPERM;
    return -1;
}
