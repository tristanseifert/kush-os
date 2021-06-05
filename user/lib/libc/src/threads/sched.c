#include <sched.h>
#include <sys/syscalls.h>

/**
 * Gives up the remainder of the CPU time.
 */
int sched_yield() {
    ThreadYield();
    return 0;
}
