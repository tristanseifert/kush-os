#ifndef KERNEL_PLATFORM_UEFI_IO_CONSOLE
#define KERNEL_PLATFORM_UEFI_IO_CONSOLE

#include <stddef.h>
#include <stdint.h>

extern "C" void _osentry(struct stivale2_struct *);

struct stivale2_struct_tag_terminal;

namespace Kernel::Vm {
class Map;
class MapEntry;
}

namespace Platform::Shared::FbCons {
class Console;
}

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

        static int SetFramebuffer(struct stivale2_struct *, Kernel::Vm::MapEntry *, void *);

    private:
        static void Init(struct stivale2_struct *);

        static void PrepareForVm(struct stivale2_struct *, Kernel::Vm::Map *);
        static void VmEnabled();

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

        /**
         * @brief VM object for framebuffer
         *
         * If the bootloader provided us with a framebuffer, it gets mapped into platform specific
         * memory space in the kernel address map.
         */
        static Kernel::Vm::MapEntry *gFb;
        /// Base address of the framebuffer
        static void *gFbBase;
        /// Width of the framebuffer (pixels)
        static size_t gFbWidth;
        /// Height of the framebuffer (pixels)
        static size_t gFbHeight;
        /// Stride of framebuffer (bytes per row)
        static size_t gFbStride;

        /**
         * @brief Instance of framebuffer console
         *
         * If the bootloader provided us with a framebuffer, we'll allocate a framebuffer console
         * (out of some memory reserved in .bss) and store it here.
         */
        static Platform::Shared::FbCons::Console *gFbCons;
};
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// re-export the console class under the generic platform namespace
namespace Platform {
    using Console = Platform::Amd64Uefi::Console;
}
#endif

#endif
