#ifndef KERNEL_LOGGING_CONSOLE_H
#define KERNEL_LOGGING_CONSOLE_H

#include <stdarg.h>
#include <stdint.h>

namespace Kernel::Logging {
/**
 * Output for kernel messages
 *
 * The console handles receiving kernel messages of various priorities and storing them in the
 * kernel's log buffer, as well as writing them to the platform console output.
 */
class Console {
        /**
         * Log priority levels
         *
         * This enumeration defines each of the console message priorities. The console may be
         * configured to drop messages below a particular priority.
         */
        enum class Priority: uint8_t {
            /// Most severe type of error
            Error                       = 5,
            /// A significant problem in the system
            Warning                     = 4,
            /// General information
            Notice                      = 3,
            /// Bonus debugging information
            Debug                       = 2,
            /// Even more verbose debugging information
            Trace                       = 1,
        };

    public:
        static void Init();

        [[noreturn]] static void Panic(const char *fmt, ...) __attribute__((format(printf,1,2)));

        /**
         * Updates the console message filter.
         *
         * @param level All messages of this level and up will be output.
         */
        static void SetFilterLevel(const Priority level) {
            gPriority = level;
        }

        /**
         * Output an error level message.
         *
         * @param fmt Format string
         * @param ... Arguments to message
         */
        static void Error(const char *fmt, ...) __attribute__((format (printf, 1, 2))) {
            va_list va;
            va_start(va, fmt);
            Log(Priority::Error, fmt, va);
            va_end(va);
        }

        /**
         * Output a warning level message.
         *
         * @param fmt Format string
         * @param ... Arguments to message
         */
        static void Warning(const char *fmt, ...) __attribute__((format (printf, 1, 2))) {
            if(static_cast<uint8_t>(gPriority) > static_cast<uint8_t>(Priority::Warning)) return;

            va_list va;
            va_start(va, fmt);
            Log(Priority::Warning, fmt, va);
            va_end(va);
        }

        /**
         * Output a notice level message.
         *
         * @param fmt Format string
         * @param ... Arguments to message
         */
        static void Notice(const char *fmt, ...) __attribute__((format (printf, 1, 2))) {
            if(static_cast<uint8_t>(gPriority) > static_cast<uint8_t>(Priority::Notice)) return;

            va_list va;
            va_start(va, fmt);
            Log(Priority::Notice, fmt, va);
            va_end(va);
        }

        /**
         * Output a debug level message.
         *
         * @param fmt Format string
         * @param ... Arguments to message
         */
        static void Debug(const char *fmt, ...) __attribute__((format (printf, 1, 2))) {
            if(static_cast<uint8_t>(gPriority) > static_cast<uint8_t>(Priority::Debug)) return;

            va_list va;
            va_start(va, fmt);
            Log(Priority::Debug, fmt, va);
            va_end(va);
        }

        /**
         * Output a trace level message.
         *
         * @param fmt Format string
         * @param ... Arguments to message
         */
        static void Trace(const char *fmt, ...) __attribute__((format (printf, 1, 2))) {
            if(static_cast<uint8_t>(gPriority) > static_cast<uint8_t>(Priority::Trace)) return;

            va_list va;
            va_start(va, fmt);
            Log(Priority::Trace, fmt, va);
            va_end(va);
        }

    private:
        static void Log(const Priority level, const char *fmt, va_list args);
        static void Log(const Priority level, const char *fmt, ...)
            __attribute__((format (printf, 2, 3)));

    private:
        /// Allow messages of this priority and up
        static Priority gPriority;

        /// Are messages sent to the platform console?
        static bool gPlatformConsoleEnabled;
};
}

// re-export it in the kernel namespace
namespace Kernel {
    using Console = Kernel::Logging::Console;
}

/// Shorthand for the kernel console panic function
#define PANIC Kernel::Logging::Console::Panic

/**
 * Ensures the given condition is true; otherwise, panics with the given message string.
 */
#define REQUIRE(cond, ...) {if(!(cond)) { Kernel::Logging::Console::Panic(__VA_ARGS__); }}

#endif
