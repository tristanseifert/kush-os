#ifndef KERNEL_PLATFORM_UEFI_IO_CONSOLE
#define KERNEL_PLATFORM_UEFI_IO_CONSOLE

#include <stddef.h>
#include <stdint.h>

extern "C" void _osentry(struct stivale2_struct *);

struct stivale2_struct_tag_terminal;

namespace Platform::Amd64Uefi {
/**
 * @brief UEFI console output
 *
 * This supports simultaneous output to the following devices:
 * - Bootloader console, provided by Sivale2 compatible loaders
 * - IO port (QEMU `debugcon`)
 * - Hardware serial port
 */
class Console {
    friend void ::_osentry(struct stivale2_struct *);

    public:
        Console() = delete;

        static void Write(const char *string, const size_t numChars);

    private:
        static void Init(struct stivale2_struct *);

        static void ParseCmd(const char *);
        static void ParseCmdToken(const char *, const size_t);

    private:
        /// The sivale2 callback for terminal output
        using TerminalWrite = void (*)(const char *str, size_t len);

        /// If non-null, the bootloader-provided terminal structure
        static struct stivale2_struct_tag_terminal *gTerminal;
        /// Function to write a string to loader terminal; only valid if `gTerminal` is also valid
        static TerminalWrite gTerminalWrite;

        /// If nonzero, the IO port to use for `debugcon` output
        static uint16_t gDebugconPort;
};
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// re-export the console class under the generic platform namespace
namespace Platform {
    using Console = Platform::Amd64Uefi::Console;
}
#endif

#endif
