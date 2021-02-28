#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

/// Kernel time info structure
struct TimeInfo {
    /// Seconds of kernel uptime
    uint32_t timeSecs;
    /// Nanoseconds component of uptime
    uint32_t timeNsec;
    /// Seconds component again; used to detect if time changed while reading
    uint32_t timeSecs2;
};

/// Pointer to kernel info structure
static const struct TimeInfo *gTimeInfo = (struct TimeInfo *) 0xBF5FD000;


/**
 * Reads out the current time according to the specified clock id.
 */
int clock_gettime(clockid_t clockId, struct timespec *tp) {
    if(!tp) {
        errno = EFAULT;
        return -1;
    }

    switch(clockId) {
        // read system uptime from time page
        case CLOCK_UPTIME_RAW: {
            uint32_t sec1, sec2, nsec;

            __atomic_load(&gTimeInfo->timeSecs, &sec1, __ATOMIC_ACQUIRE);
            __atomic_load(&gTimeInfo->timeNsec, &nsec, __ATOMIC_ACQUIRE);
            __atomic_load(&gTimeInfo->timeSecs2, &sec2, __ATOMIC_ACQUIRE);

            // if sec2 is different than sec1, it will be newer, so use it, and reset nsec
            if(sec1 != sec2) {
                nsec = 0;
                sec1 = sec2;
            }

            // TODO: apply an offset via RDTSC

            // write to output
            tp->tv_sec = sec1;
            tp->tv_nsec = nsec;
            return 0;
        }

        // unsupported clock
        default:
            fprintf(stderr, "%s (clock %u) unimplemented\n", __PRETTY_FUNCTION__, clockId);
            errno = EINVAL;
            return -1;
    }

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
