#ifndef KERNEL_LOGGING_CONSOLE_H
#define KERNEL_LOGGING_CONSOLE_H

namespace Kernel::Logging {
/**
 * Output for kernel messages
 *
 * The console handles receiving kernel messages of various priorities and storing them in the
 * kernel's log buffer, as well as writing them to the platform console output.
 */
class Console {
    public:
        /**
         * Log priority levels
         *
         * This enumeration defines each of the console message priorities. The console may be
         * configured to drop messages below a particular priority.
         */
        enum class Priority {
            /// Most severe type of error
            Error,
            /// A significant problem in the system
            Warning,
            /// General information
            Notice,
            /// Bonus debugging information
            Debug,
            /// Even more verbose debugging information
            Trace,
        };

        static void Init();

        static void Log(const Priority level, const char *fmt, ...)
            __attribute__((format (printf, 2, 3)));

        [[noreturn]] static void Panic(const char *fmt, ...) __attribute__((format(printf,1,2)));

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

#endif
