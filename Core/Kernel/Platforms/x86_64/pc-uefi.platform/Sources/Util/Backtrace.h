#ifndef KERNEL_PLATFORM_UEFI_UTIL_BACKTRACE
#define KERNEL_PLATFORM_UEFI_UTIL_BACKTRACE

#include <stddef.h>

namespace Platform::Amd64Uefi {
/**
 * Implements support for walking stacks and secreting backtraces.
 */
class Backtrace {
    public:
        static int Print(const void *stack, char *outBuf, const size_t outBufLen,
                const bool symbolicate = false);
};
}

// re-export in platform namespace
namespace Platform {
using Backtrace = Platform::Amd64Uefi::Backtrace;
}

#endif
