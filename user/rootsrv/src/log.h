#ifndef LOG_H
#define LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Writes a log message to the kernel log buffer.
 */
void log(const char *format, ...) __attribute__((format (printf, 1, 2)));


#ifdef __cplusplus
}
#endif


/**
 * Ensures the given condition is true; otherwise, panics with the given message string.
 */
#define REQUIRE(cond, ...) {if(!(cond)) { panic(__VA_ARGS__); }}

#endif
