#include "log.h"

#include <printf.h>
#include <sys/syscalls.h>
#include <string.h>

/**
 * Writes the message to the kernel log.
 *
 * This will be an in-memory buffer, as well as optionally a debug spew port defined by the
 * platform code.
 */
void log(const char *format, ...) {
    va_list va;
    va_start(va, format);

    static const size_t kLogBufSz = 2048;
    static char logBuf[2048];
    memset(logBuf, 0, kLogBufSz);

    int written = vsnprintf(logBuf, kLogBufSz, format, va);

    va_end(va);

    // add a newline
    /*if(written < kLogBufSz) {
        logBuf[written] = '\n';
        written++;
    }*/

    // do debug print
    DbgOut(logBuf, written);
}
