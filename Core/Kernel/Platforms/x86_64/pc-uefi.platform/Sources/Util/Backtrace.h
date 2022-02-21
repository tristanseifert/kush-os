#ifndef KERNEL_PLATFORM_UEFI_UTIL_BACKTRACE
#define KERNEL_PLATFORM_UEFI_UTIL_BACKTRACE

#include <stddef.h>
#include <stdint.h>

extern "C" void _osentry(struct stivale2_struct *);

namespace Platform::Amd64Uefi {
/**
 * Implements support for walking stacks and secreting backtraces.
 */
class Backtrace {
    friend void ::_osentry(struct stivale2_struct *);

    public:
        static int Print(const void *stack, char *outBuf, const size_t outBufLen,
                const bool symbolicate = false);

        static int Symbolicate(const uintptr_t pc, char *outBuf, const size_t outBufLen);

    private:
        static void Init(struct stivale2_struct *);
};
}

// re-export in platform namespace
namespace Platform {
using Backtrace = Platform::Amd64Uefi::Backtrace;
}

#endif
