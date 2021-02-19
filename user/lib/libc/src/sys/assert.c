#include <stdint.h>
#include <printf.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/syscalls.h>

/**
 * Assertion failure handler
 */
void __assert(const char *func, const char *file, int line, const char *expr) {
    static const size_t kAssertBufSz = 2048;
    static char assertBuf[2048];
    memset(assertBuf, 0, kAssertBufSz);

    int written = snprintf(assertBuf, kAssertBufSz, 
            "Assertion failed: (%s), in %s (%s:%d)\n", expr, func, file,
            line);

    // do debug print
    DbgOut(assertBuf, written);

    abort();
}
