#ifndef LOG_H
#define LOG_H

/// Outputs a message if trace logging is enabled
void Trace(const char * _Nonnull format, ...) __attribute__ ((format (printf, 1, 2)));
/// Outputs a success message
void Success(const char * _Nonnull format, ...) __attribute__ ((format (printf, 1, 2)));
/// Outputs an informational message
void Info(const char * _Nonnull format, ...) __attribute__ ((format (printf, 1, 2)));
/// Outputs a warning message
void Warn(const char * _Nonnull format, ...) __attribute__ ((format (printf, 1, 2)));
/// Outputs an error message and exits the task.
[[noreturn]] void Abort(const char * _Nonnull format, ...) __attribute__ ((format (printf, 1, 2)));

/// Ensure the given condition is true, otherwise aborts.
#define REQUIRE(cond, ...) {if(!(cond)) { Abort(__VA_ARGS__); }}

#endif
