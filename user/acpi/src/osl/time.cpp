/*
 * Implementations of the ACPICA OS layer
 */
#include "osl.h"
#include "log.h"

#include <ctime>
#include <errno.h>

#include <acpi.h>

/**
 * Returns the system timer value, in 100ns increments.
 */
UINT64 AcpiOsGetTimer() {
    /*uint64_t ret;
    asm volatile ( "rdtsc" : "=A"(ret) );
    return ret;*/

    
    struct timespec tm;
    int err = clock_gettime(CLOCK_UPTIME_RAW, &tm);

    if(err) {
        Abort("%s failed: %d", "clock_gettime", errno);
    }

    const uint64_t nsec = (tm.tv_nsec + (tm.tv_sec * 1000000000ULL));
    return nsec / 100ULL;
}
