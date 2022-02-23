#ifndef KERNEL_PLATFORM_UEFI_BOOT_HELPERS
#define KERNEL_PLATFORM_UEFI_BOOT_HELPERS

#include <stivale2.h>

namespace Platform::Amd64Uefi {
/**
 * Helpers for working with Stivale2 bootloaders.
 */
struct Stivale2 {
    Stivale2() = delete;

    /**
     * Searches through all tags specified in the given bootloader info structure for a given id.
     *
     * @param info Stivale2 loader info (passed into entry point)
     * @param id The tag id to search for
     *
     * @return Start of the tag's info struct, or `nullptr` if not found
     */
    static struct stivale2_tag *GetTag(const stivale2_struct *info, const uint64_t id) {
        auto current = reinterpret_cast<struct stivale2_tag *>(info->tags);
        for(;;) {
            if(!current) {
                return nullptr;
            }

            if(current->identifier == id) {
                return current;
            }

            current = reinterpret_cast<struct stivale2_tag *>(current->next);
        }
    }
};
}

#endif
