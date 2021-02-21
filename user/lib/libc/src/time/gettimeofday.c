#include <errno.h>
#include <time.h>
#include <sys/time.h>

/**
 * Returns the current time.
 *
 * This is done entirely in userspace, by reading the kernel's time out of the kernel time
 * segment. Then, depending on the processor's capabilities, we add to it based on some userspace
 * high resolution timer.
 */
int gettimeofday(struct timeval *tv, struct timezone *tz) {
    // TODO: implement
    errno = ENXIO;
    return -1;
}
