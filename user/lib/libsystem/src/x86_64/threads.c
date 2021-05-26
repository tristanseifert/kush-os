#include <sys/syscalls.h>
#include <stdint.h>

/**
 * Prepare the stack frame of a thread.
 *
 * We'll stack on first the argument, then the function to invoke.
 */
void LIBSYSTEM_INTERNAL __ThreadStackPrepare(const uintptr_t stack, void (*entry)(uintptr_t), const uintptr_t arg) {
    uintptr_t *args = (uintptr_t *) stack;
    args[-1] = arg;
    args[-2] = (uintptr_t) entry;
}
