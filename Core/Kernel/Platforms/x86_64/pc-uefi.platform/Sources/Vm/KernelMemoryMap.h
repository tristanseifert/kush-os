#ifndef KERNEL_PLATFORM_UEFI_KERNELMEMORYMAP_H
#define KERNEL_PLATFORM_UEFI_KERNELMEMORYMAP_H

#include <stddef.h>
#include <stdint.h>

namespace Platform::Amd64Uefi {
/**
 * @brief Definitions for the kernel address space
 *
 * This enum defines the base (and end) addresses of various regions of the kernel address space,
 * and are used by the platform-agnostic code to decide where to map stuff.
 *
 * On amd64, all addresses above `0x8000'0000'0000'0000` are reserved for the kernel; however, most
 * processors only implement 48-bit virtual address spaces, and addresses must be cannonical. This
 * means that the kernel region really starts at `0xffff'8000'0000'0000`.
 */
enum KernelAddressLayout: uintptr_t {
    /**
     * Kernel virtual address split
     *
     * Addresses below this split belong to userspace.
     */
    KernelBoundary                                      = 0x8000'0000'0000'0000,

    /**
     * Physical aperture
     *
     * Start of the region used to indirectly access physical memory. It is mapped as read/write
     * and used to modify page tables, etc.
     */
    PhysApertureStart                                   = 0xffff'8000'0000'0000,
    /// End of physical aperture
    PhysApertureEnd                                     = 0xffff'81ff'ffff'ffff,

    /**
     * Physical allocator metadata
     *
     * Start of the physical allocator's metadata region. This is a 1G chunk of virtual memory
     * reserved for the bitmaps indicating what pages are free.
     */
    PhysAllocatorMetadataStart                          = 0xffff'8200'0000'0000,
    /// End of the physical allocator metadata
    PhysAllocatorMetadataEnd                            = 0xffff'8200'3fff'ffff,

    /**
     * Kernel file image
     *
     * Memory mapped view of the entire kernel image file, as read from the boot medium by the
     * bootloader. This is primarily used to extract strings for symbolication of kernel symbols
     * in backtraces.
     */
    KernelImageStart                                    = 0xffff'8200'4000'0000,
    /// End of the kernel file image
    KernelImageEnd                                      = 0xffff'8200'41ff'0000,

    /**
     * Kernel executable start
     *
     * This marks the start of the kernel's executable region. The actual executable start can be
     * slid anywhere within this region for ASLR purposes.
     */
    KernelExecStart                                     = 0xffff'ffff'8000'0000,
    /// End of the kernel executable region
    KernelExecEnd                                       = 0xffff'ffff'ffff'ffff,
};
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace Platform {
using KernelAddressLayout = Platform::Amd64Uefi::KernelAddressLayout;
}
#endif

#endif
