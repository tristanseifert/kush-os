#include "multiboot.h"
#include "multiboot2.h"
#include "InitTask.h"
#include "physmap.h"

#include "acpi/Manager.h"

#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <log.h>

using namespace platform;

// buffer for kernel command line
constexpr static const size_t kCmdlineLen = 256;
static char x86_cmdline_buf[kCmdlineLen];
// buffer for bootloader name
constexpr static const size_t kLoaderNameLen = 64;
static char x86_loader_name[kLoaderNameLen];

/// physical address of the multiboot structure
uint32_t x86_multiboot_info = 0;
uint32_t x86_multiboot_magic = 0;

/**
 * Parse the multiboot structure.
 */
void multiboot_parse() {
    struct multiboot_tag *tag;

    // validate info
    REQUIRE(x86_multiboot_magic == MULTIBOOT2_BOOTLOADER_MAGIC, "invalid multiboot2 magic: $%08x (multiboot at %08x)", x86_multiboot_magic, x86_multiboot_info);
    REQUIRE(x86_multiboot_info < 0x40000000, "Multiboot info must be in low 1G (is at $%08x)", x86_multiboot_info);

    // iterate over all tags
    for(tag = (struct multiboot_tag *) (x86_multiboot_info + 8);
       tag->type != MULTIBOOT_TAG_TYPE_END;
       tag = (struct multiboot_tag *) ((uint8_t *) tag + ((tag->size + 7) & ~7))) {
        // handle tag type
        switch(tag->type) {
            // kernel command line
            case MULTIBOOT_TAG_TYPE_CMDLINE: {
                auto str = (struct multiboot_tag_string *) tag;
                strncpy(x86_cmdline_buf, str->string, kCmdlineLen);
                break;
            }
            // bootloader name
            case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME: {
                auto str = (struct multiboot_tag_string *) tag;
                strncpy(x86_loader_name, str->string, kLoaderNameLen);
                break;
            }
            // memory map
            case MULTIBOOT_TAG_TYPE_MMAP: {
                auto mmap = (struct multiboot_tag_mmap *) tag;
                physmap_load_from_multiboot(mmap);
                break;
            }
            // ACPI info (32-bit RSDP)
            case MULTIBOOT_TAG_TYPE_ACPI_OLD: {
                auto info = (struct multiboot_tag_old_acpi *) tag;
                acpi::Manager::init(info);
                break;
            }
            case MULTIBOOT_TAG_TYPE_ACPI_NEW: {
                auto info = (struct multiboot_tag_old_acpi *) tag;
                acpi::Manager::init(info);
                break;
            } 
                //panic("New style ACPI table not supported");
                //break;

            // modules provided by bootloader
            case MULTIBOOT_TAG_TYPE_MODULE: {
                auto info = reinterpret_cast<struct multiboot_tag_module *>(tag);
                // figure out the module type
                uint32_t type = 0;
                strncpy(reinterpret_cast<char *>(&type), info->cmdline, sizeof(type));

                type = __builtin_bswap32(type);

                InitHandleModule(type, info->mod_start, info->mod_end, info->cmdline);
                break;
            }

            // unknown tag
            default:
                // log("unhandled multiboot type %d (len %d)", tag->type, tag->size);
                break;
        }
    }
}

