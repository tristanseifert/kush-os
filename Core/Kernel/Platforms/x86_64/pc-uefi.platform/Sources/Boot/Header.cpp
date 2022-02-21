/**
 * Define the stivale2 information structure.
 *
 * It is placed in a dedicated section, which the linker script will keep.
 */
#include <stdint.h>
#include <stddef.h>
#include <stivale2.h>

/**
 * Stack for the boot processor.
 */
static uint8_t gBspStack[8192] __attribute__((aligned(64)));

// Ignore warnings from unused tags.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

/**
 * Unmap the first page of virtual address space to trap NULL dereferences.
 */
static stivale2_tag gUnmapNullTag = {
    .identifier = STIVALE2_HEADER_TAG_UNMAP_NULL_ID,
    // end of tag list
    .next = 0
};

/**
 * Slide higher half: Have the bootloader apply a slide to the base address of the kernel, with
 * a 2MB slide alignment.
 */
static struct stivale2_header_tag_slide_hhdm gSlideTag = {
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_SLIDE_HHDM_ID,
        .next = reinterpret_cast<uintptr_t>(&gUnmapNullTag),
    },
    // reserved
    .flags = 0,
    // alignment of the slide
    .alignment = 0x200000,
};

/**
 * Terminal header tag: Enable the built-in terminal from the bootloader. This is used for early
 * boot IO.
 */
static struct stivale2_header_tag_terminal gTerminalTag = {
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_TERMINAL_ID,
        //.next = reinterpret_cast<uintptr_t>(&gSlideTag),
        .next = reinterpret_cast<uintptr_t>(&gUnmapNullTag),
    },
    // reserved
    .flags = 0
};

/**
 * Framebuffer tag: request that the bootloader places the system's graphics hardware into a
 * graphical mode, rather than text mode.
 */
static struct stivale2_header_tag_framebuffer gFramebufferTag = {
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
        .next = reinterpret_cast<uintptr_t>(&gTerminalTag),
    },
    // the bootloader shall pick the best resolution/bpp
    .framebuffer_width  = 0,
    .framebuffer_height = 0,
    .framebuffer_bpp    = 0
};

#pragma GCC diagnostic pop

/**
 * This is the main bootloader information block. This has to live in its own section so that the
 * bootloader can read it.
 */
__attribute__((section(".stivale2hdr"), used))
static struct stivale2_header gStivaleHeader = {
    // use ELF entry point
    .entry_point = 0,
    /*
     * Specify the bottom of the stack. This is only used for the boot processor, and even then,
     * only until the scheduler is started.
     */
    .stack = reinterpret_cast<uintptr_t>(gBspStack) + sizeof(gBspStack),

    /*
     * Loader flag bits:
     * - Bit 1: Get pointers to higher half
     * - Bit 2: Enable protected memory ranges (apply ELF PHDR protections)
     * - Bit 3: Map kernel wherever it fits physically
     * - Bit 4: Always set
     */
    .flags = 0b00011111,

    // point to the first of our tags
    .tags = reinterpret_cast<uintptr_t>(&gFramebufferTag),
};
