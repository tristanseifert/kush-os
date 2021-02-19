#include <stdint.h>
#include <stdlib.h>

#if UINT32_MAX == UINTPTR_MAX
#define STACK_CHK_GUARD 0xe2dee396
#else
#define STACK_CHK_GUARD 0x595e9fbd94fda766
#endif

/// Define the stack protector value
uintptr_t __attribute__((used)) __stack_chk_guard = STACK_CHK_GUARD;

void __stack_chk_fail(void);


/**
 * Invoked when stack bounds check fails; this just panics the machine
 */
__attribute__((noreturn)) void __stack_chk_fail(void) {
    abort();
}
