#ifndef LIBSYSTEM_BACKTRACE_H
#define LIBSYSTEM_BACKTRACE_H

#include <_libsystem.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Prints into the given character buffer a stack trace, with the provided stack/frame pointer
 * used as a base.
 *
 * @param stack Stack or base pointer, or use `NULL` to retrieve it
 * @param buf Output character buffer
 * @param bufLen Maximum number of bytes to write to the buffer.
 *
 * @return Number of bytes written to the buffer or a negative error code
 */
LIBSYSTEM_EXPORT int BacktracePrint(void *stack, char *buf, const size_t bufLen);


#ifdef __cplusplus
}
#endif
#endif
