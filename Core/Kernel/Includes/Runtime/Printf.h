/**
 * Re-declare the printf() family functions in the kernel that are friendly to be used from other
 * code.
 */
#ifndef KERNEL_RUNTIME_PRINTF_H
#define KERNEL_RUNTIME_PRINTF_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __GNUC__
# define ATTR_PRINTF(one_based_format_index, first_arg) \
__attribute__((format(__printf__, (one_based_format_index), (first_arg))))
# define ATTR_VPRINTF(one_based_format_index) ATTR_PRINTF((one_based_format_index), 0)
#else
# define ATTR_PRINTF((one_based_format_index), (first_arg))
# define ATTR_VPRINTF(one_based_format_index)
#endif

#ifdef __cplusplus
extern "C" {
#endif

int  snprintf_(char* s, size_t count, const char* format, ...) ATTR_PRINTF(3, 4);
int vsnprintf_(char* s, size_t count, const char* format, va_list arg) ATTR_VPRINTF(3);
int fctprintf(void (*out)(char c, void* extra_arg), void* extra_arg, const char* format, ...) ATTR_PRINTF(3, 4);
int vfctprintf(void (*out)(char c, void* extra_arg), void* extra_arg, const char* format, va_list arg) ATTR_VPRINTF(3);

#ifdef __cplusplus
}
#endif

#endif
