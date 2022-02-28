#ifndef KERNEL_PLATFORM_UEFI_MEMORY_PHYSICALMAP_H
#define KERNEL_PLATFORM_UEFI_MEMORY_PHYSICALMAP_H

#include <stddef.h>
#include <stdint.h>

namespace Platform::Amd64Uefi::Memory {
/**
 * @brief Translate physical to virtual memory addresses via the physical memory aperture
 *
 * Since we have a shitload of bonus virtual address space on 64-bit platform, we just reserve a
 * bunch of it for a physical aperture. Then, when a request comes in to perform this mapping, we
 * don't have to do anything.
 */
class PhysicalMap {
    public:
        static int Add(const uintptr_t physical, const size_t length, void **outVirtual);
        static int Remove(const void *virtualAddr, const size_t length);

    private:
        /**
         * Indicates whether we're using the early boot mappings.
         *
         * During early boot, we're operating on page tables set up by the bootloader. These page
         * tables have at least the low 4GB identity mapped. The flag is cleared once the full VM
         * system is set up and we have more memory available.
         */
        static bool gIsEarlyBoot;
};
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace Platform::Memory {
using PhysicalMap = Platform::Amd64Uefi::Memory::PhysicalMap;
}
#endif

#endif
