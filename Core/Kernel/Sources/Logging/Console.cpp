#include "Logging/Console.h"
#include "BuildInfo.h"

#include <printf/printf.h>
#include <Platform.h>

#include <stdarg.h>

using namespace Kernel::Logging;

Console::Priority gPriority{Console::Priority::Notice};
bool Console::gPlatformConsoleEnabled{true};

/**
 * Dummy putchar implementation; this doesn't do anything but shuts up linker errors. Nobody should
 * be using printf() anyways
 */
void putchar_(char) {
    PANIC("putchar called");
}

/**
 * Initializes the kernel console.
 *
 * @remark You must not send any log messages to the console before this call is made; this should
 *         be done very early in the platform initialization code.
 */
void Console::Init() {
    Log(Priority::Notice, "kush-os (%s@%s) %s\nBuilt on %s by %s@%s", gBuildInfo.gitHash,
            gBuildInfo.gitBranch, gBuildInfo.buildType, gBuildInfo.buildDate, gBuildInfo.buildUser,
            gBuildInfo.buildHost);
    Log(Priority::Notice, "Active platform: %s (%s)", gBuildInfo.platform, gBuildInfo.arch);
}

/**
 * Writes a message with the specified severity to the console output.
 *
 * @param level Priority level of the message. Messages with a level lower than the current filter
 *        are discarded.
 * @param fmt A printf-style format string
 * @param ... Arguments to the format string
 */
void Console::Log(const Priority level, const char *fmt, ...) {
    va_list va;

    // TODO: check log severity

    // format the message (TODO: do not use a static buffer!)
    static constexpr const size_t kBufChars{1024};
    static char buf[kBufChars];

    va_start(va, fmt);
    int chars = vsnprintf_(buf, kBufChars-1, fmt, va);
    va_end(va);

    // append a newline
    buf[chars++] = '\n';

    // punt it to the output methods
    if(gPlatformConsoleEnabled) {
        Platform::Console::Write(buf, chars);
    }
}

/**
 * Write a panic message to the console, then halt the system.
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
    Log(Priority::Error, "\n\033[101;97mPANIC: %s\033[0m\nPC = %p\n", panicMsgBuf, pc);

    panicMsgBuf[0] = '\0';
    Platform::Backtrace::Print(nullptr, panicMsgBuf, kMsgBufChars, true);
    Log(Priority::Error, "Backtrace:%s", panicMsgBuf);

    // halt machine
    Platform::Processor::HaltAll();
}

