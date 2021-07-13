#include "Log.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

extern const char *gLogTag;

#ifdef NDEBUG
static bool gLogTraceEnabled = false;
#else
static bool gLogTraceEnabled = true;
#endif

/// Outputs a message if trace logging is enabled
void Trace(const char * _Nonnull str, ...) {
    if(!gLogTraceEnabled) return;
    fprintf(stdout, "\e[34m[%s] ", gLogTag);

    va_list va;
    va_start(va, str);
    vfprintf(stdout, str, va);
    va_end(va);

    fputs("\e[0m\n", stdout);
}
/**
 * Logs a success message.
 */
void Success(const char *str, ...) {
    fprintf(stderr, "\e[32m[%s] ", gLogTag);

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
    fprintf(stderr, "[%s] ", gLogTag);

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
    fprintf(stderr, "\e[33m[%s] ", gLogTag);

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
    fprintf(stderr, "\e[31m[%s] ", gLogTag);

    va_list va;
    va_start(va, str);
    vfprintf(stderr, str, va);
    va_end(va);

    fputs("\e[0m\n", stderr);
    _Exit(-69);
}

