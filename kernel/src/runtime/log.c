#include <log.h>
#include <platform.h>
#include <printf.h>

/**
 * printf wrapper to send a character to the debug spew port
 */
static void _outchar(char ch, void *ctx) {
    platform_debug_spew(ch);
}

/**
 * Writes the message to the kernel log.
 *
 * This will be an in-memory buffer, as well as optionally a debug spew port defined by the
 * platform code.
 */
void log(const char *format, ...) {
    va_list va;
    va_start(va, format);

    fctvprintf(_outchar, NULL, format, va);

    va_end(va);

    platform_debug_spew('\n');
}

/**
 * Take the panic lock, write the message, then halt the system.
 */
void panic(const char *format, ...) {
    void *pc = __builtin_return_address(0);

    // take panic lock

    // set up panic buffer
    static const size_t kPanicBufSz = 2048;
    static char panicBuf[2048];

    // format the message into it
    va_list va;
    va_start(va, format);

    vsnprintf(panicBuf, kPanicBufSz, format, va);

    va_end(va);

    // output it
    fctprintf(_outchar, NULL, "panic: %s\npc: $%p", panicBuf, pc);

    // then jump to the platform panic handler
    platform_panic_handler();
}
