/**
 * Stack smashing protection
 *
 * Implements the error handler and stack guard variable. The latter is exported so that the
 * platform's entry point can set a random stack cookie value.
 */
#include <stdint.h>

#include "Logging/Console.h"

uintptr_t __stack_chk_guard __attribute__((used)) = 0x4EC170E9FBB9303B;

/**
 * Stack smashing error handler
 *
 * This is invoked when a stack cookie fails verification.
 *
 * TODO: Switch to a different stack? May not be secure
 */
extern "C" [[noreturn]] void __stack_chk_fail(void) {
    PANIC("Stack corruption detected!");
}
