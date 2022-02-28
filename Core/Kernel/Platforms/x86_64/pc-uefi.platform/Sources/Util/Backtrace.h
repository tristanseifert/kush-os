#ifndef KERNEL_PLATFORM_UEFI_UTIL_BACKTRACE
#define KERNEL_PLATFORM_UEFI_UTIL_BACKTRACE

#include <stddef.h>
#include <stdint.h>

extern "C" void _osentry(struct stivale2_struct *);

namespace Platform::Amd64Uefi {
/**
 * @brief amd64 stack walking and backtrace generation
 *
 * If the bootloader provides us the location of the full kernel file image in memory, we try to
 * parse the ELF sufficiently to read out the location of the string table, in order to symbolicate
 * backtraces.
 */
class Backtrace {
    friend void ::_osentry(struct stivale2_struct *);

    public:
        Backtrace() = delete;

        static int Print(const void *stack, char *outBuf, const size_t outBufLen,
                const bool symbolicate = false, const size_t skip = 0);

        static int Symbolicate(const uintptr_t pc, char *outBuf, const size_t outBufLen);

    private:
        static void Init(struct stivale2_struct *);

        /// Start of the ELF symbol table
        static const void *gSymtab;
        /// Size of the ELF symbol table, in bytes
        static size_t gSymtabLen;

        /// Start of the ELF string table (for symbol names)
        static const char *gStrtab;
        /// Length of the ELF string table
        static size_t gStrtabLen;
};
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// re-export in platform namespace
namespace Platform {
using Backtrace = Platform::Amd64Uefi::Backtrace;
}
#endif

#endif
