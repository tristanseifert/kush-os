#include "Logging/Console.h"
#include "Runtime/Printf.h"
#include "BuildInfo.h"

#include <Platform.h>

#include <stdarg.h>

using namespace Kernel::Logging;

Console::Priority Console::gPriority{Console::Priority::Notice};
bool Console::gPlatformConsoleEnabled{true};

/**
 * Dummy putchar implementation; this doesn't do anything but shuts up linker errors. Nobody should
 * be using printf() anyways
 */
void putchar_(char) {
    PANIC("putchar called");
}

/**
 * @brief Initializes the kernel console.
 *
 * @remark You must not send any log messages to the console before this call is made; this should
 *         be done very early in the platform initialization code.
 */
void Console::Init() {
    Notice("kush-os (%s@%s) %s\nBuilt on %s by %s@%s", gBuildInfo.gitHash,
            gBuildInfo.gitBranch, gBuildInfo.buildType, gBuildInfo.buildDate, gBuildInfo.buildUser,
            gBuildInfo.buildHost);
    Notice("Active platform: %s (%s)", gBuildInfo.platform, gBuildInfo.arch);
}

/**
 * @brief Writes a message with the specified severity to the console output.
 *
 * @param level Priority level of the message. Messages with a level lower than the current filter
 *        are discarded.
 * @param fmt A printf-style format string
 * @param ... Arguments to the format string
 */
void Console::Log(const Priority level, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    Log(level, fmt, va);
    va_end(va);
}

/**
 * @brief Writes a message with the specified severity to the console output.
 *
 * @remark This does not validate whether the message is to be filtered out.
 *
 * @param level Priority level of the message. Messages with a level lower than the current filter
 *        are discarded.
 * @param fmt A printf-style format string
 * @param va Captured variadic arguments for format message
 */
void Console::Log(const Priority level, const char *fmt, va_list va) {
    // format the message (TODO: do not use a static buffer!)
    static constexpr const size_t kBufChars{1024};
    static char buf[kBufChars];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    int chars = vsnprintf_(buf, kBufChars-1, fmt, va);
#pragma GCC diagnostic pop

    // append a newline
    buf[chars++] = '\n';

    // punt it to the output methods
    if(gPlatformConsoleEnabled) {
        Platform::Console::Write(buf, chars);
    }
}

/**
 * @brief Write a panic message to the console, then halt the system.
 */
void Console::Panic(const char *fmt, ...) {
    va_list va;
    void *pc = __builtin_return_address(0);

    // format panic message
    static constexpr const size_t kMsgBufChars{1024};
    static char panicMsgBuf[kMsgBufChars];

    va_start(va, fmt);
    vsnprintf_(panicMsgBuf, kMsgBufChars, fmt, va);
    va_end(va);

    // then output it, and the backtrace
    Error("\n\033[101;97mPANIC: %s\033[0m\nPC = %p\n", panicMsgBuf, pc);

    panicMsgBuf[0] = '\0';
    Platform::Backtrace::Print(nullptr, panicMsgBuf, kMsgBufChars, true, 1);
    Error("Backtrace:%s", panicMsgBuf);

    // halt machine
    Hang();
}

/**
 * Hang the machine after a panic.
 *
 * This is a separate method so it shows up easier in backtraces.
 */
__attribute__((noinline))
void Console::Hang() {
    Platform::Processor::HaltAll();
}
