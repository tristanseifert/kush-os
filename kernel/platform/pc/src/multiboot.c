#include "multiboot.h"
#include "multiboot2.h"

#include "physmap.h"

#include <stddef.h>
#include <stdint.h>
#include <log.h>

/// physical address of the multiboot structure
uint32_t x86_multiboot_info = 0;
uint32_t x86_multiboot_magic = 0;

/**
 * Parse the multiboot structure.
 */
void multiboot_parse() {
    struct multiboot_tag *tag;

    // validate info
    REQUIRE(x86_multiboot_magic == MULTIBOOT2_BOOTLOADER_MAGIC, "invalid multiboot2 magic: $%08lx", x86_multiboot_magic);
    REQUIRE(x86_multiboot_info < 0x400000, "Multiboot info must be in low 4M (is at $%08lx)", x86_multiboot_info);

    // get the total size of the parameter region
    const size_t totalParamsBytes = *((uint32_t *) x86_multiboot_info);
    log("Multiboot info size %lu", totalParamsBytes);

    // iterate over all tags
    for(tag = (struct multiboot_tag *) (x86_multiboot_info + 8);
       tag->type != MULTIBOOT_TAG_TYPE_END;
       tag = (struct multiboot_tag *) ((uint8_t *) tag + ((tag->size + 7) & ~7))) {
        // handle tag type
        switch(tag->type) {
            // memory map
            case MULTIBOOT_TAG_TYPE_MMAP: {
                struct multiboot_tag_mmap *mmap = (struct multiboot_tag_mmap *) tag;
                physmap_load_from_multiboot(mmap);
                break;
            }

            // unknown tag
            default:
                // log("unhandled multiboot type %d (len %d)", tag->type, tag->size);
                break;
        }
    }

    // physmap_load_from_multiboot((void *) x86_multiboot_info);
}
