#include "log.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#ifdef NDEBUG
static bool gLogTraceEnabled = false;
#else
static bool gLogTraceEnabled = true;
#endif

/// Outputs a message if trace logging is enabled
void Trace(const char * _Nonnull str, ...) {
    if(!gLogTraceEnabled) return;
    fputs("\e[34m[acpi] ", stderr);

    va_list va;
    va_start(va, str);
    vfprintf(stdout, str, va);
    va_end(va);

    fputs("\e[0m\n", stderr);
}
/**
 * Logs a success message.
 */
void Success(const char *str, ...) {
    fputs("\e[32m[acpi] ", stderr);

    va_list va;
    va_start(va, str);
    vfprintf(stderr, str, va);
    va_end(va);

    fputs("\e[0m\n", stderr);
}
/**
 * Logs an informational message.
 */
void Info(const char *str, ...) {
    fputs("[acpi] ", stderr);

    va_list va;
    va_start(va, str);
    vfprintf(stderr, str, va);
    va_end(va);

    fputs("\n", stderr);
}
/**
 * Logs a warning message.
 */
void Warn(const char *str, ...) {
    fputs("\e[33m[acpi] ", stderr);

    va_list va;
    va_start(va, str);
    vfprintf(stderr, str, va);
    va_end(va);

    fputs("\e[0m\n", stderr);
}
/**
 * Logs an error and terminates the task.
 */
void Abort(const char *str, ...) {
    fputs("\e[31m[acpi] ", stderr);

    va_list va;
    va_start(va, str);
    vfprintf(stderr, str, va);
    va_end(va);

    fputs("\e[0m\n", stderr);
    _Exit(-69);
}

