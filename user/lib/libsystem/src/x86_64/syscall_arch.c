#include "../sys/syscall.h"
#include <sys/syscalls.h>
#include <sys/amd64/syscalls.h>

#include <stdint.h>
#include <stdbool.h>

/**
 * Updates the base of the %fs register for the thread.
 */
int Amd64SetThreadLocalBase(const int which, const uintptr_t base) {
    return Amd64SetThreadLocalBaseFor(0, which, base);
}

/**
 * Updates the base of either the %fs or %gs register,
 */
int Amd64SetThreadLocalBaseFor(const uintptr_t threadHandle, const int which, const uintptr_t base) {
    return __do_syscall3(threadHandle, (which == SYS_ARCH_AMD64_TLS_GS) ? true : false, base,
            SYS_ARCH_AMD64_SET_FGS_BASE);
}

/**
 * Called when a thread returns; it'll gracefully destroy this one.
 */
void Amd64ThreadExit() {
    int err;
    uintptr_t handle;

    // get handle
    err = ThreadGetHandle(&handle);
    if(err) {
        asm volatile("ud2" ::: "memory");
    }

    // then destroy
    ThreadDestroy(handle);
}
