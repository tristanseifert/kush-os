#ifndef KERNEL_LOG_H
#define KERNEL_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Writes a log message to the kernel log buffer.
 */
void log(const char *format, ...) __attribute__((format (printf, 1, 2)));

/**
 * Formats the given message to the output then halts the system.
 */
void panic(const char *format, ...) __attribute__((format (printf, 1, 2))) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif


#endif
