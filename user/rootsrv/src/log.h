#ifndef LOG_H
#define LOG_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Writes a log message to the kernel log buffer.
 */
void LOG(const char *format, ...) __attribute__((format (printf, 1, 2)));


#ifdef __cplusplus
}
#endif


/**
 * Ensures the given condition is true; otherwise, panics with the given message string.
 */
#define REQUIRE(cond, ...) {if(!(cond)) { LOG(__VA_ARGS__); abort(); }}

#endif
