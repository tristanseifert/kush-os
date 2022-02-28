#ifndef KERNEL_PLATFORM_UEFI_ARCH_GDTTYPES_H
#define KERNEL_PLATFORM_UEFI_ARCH_GDTTYPES_H

#include <stdint.h>

namespace Platform::Amd64Uefi {
/**
 * @brief 32-bit GDT entry
 */
struct GdtDescriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

/**
 * @brief 64-bit GDT entry
 *
 * The extended GDT entr format is used when the system bit (bit 4 of the Access flags) is clear,
 * forming a 16-bit descriptor. This contains a full 64-bit pointer, and can be used for TSS
 * segments. (Code/data segments in long mode are ignored.)
 */
struct GdtDescriptor64 {
    /// limit 15..0
    uint16_t limit0;
    /// base 15..0
    uint16_t base0;
    /// base 23..16
    uint8_t base1;
    /// present flag, DPL, type
    uint8_t typeFlags;
    /// granularity, available flag, bits 19..16 of limit
    uint8_t granularityLimit;
    /// base address 31..24
    uint8_t base2;
    /// base address 63..32
    uint32_t base3;
    /// reserved (always zero)
    uint32_t reserved;
} __attribute__((packed));


/**
 * @brief Task state structure
 *
 * Task state structure for amd64 mode. The only part of this structure that we really care about
 * (and use) are the interrupt stacks.
 *
 * @note Take special care when initializing a TSS, particularly the IOPB field: failure to do so
 *       can cause  <a href="https://www.os2museum.com/wp/the-history-of-a-security-hole/">security
 *       problems.</a>
 *
 * @remark All reserved fields should be initialized to zero.
*/
struct Tss {
    uint32_t reserved1;

    /// stack pointers (RSP0 - RSP2)
    struct {
        uint32_t low, high;
    } __attribute__((packed)) rsp[3];

    uint32_t reserved2[2];

    /// interrupt stacks
    struct {
        uint32_t low, high;
    } __attribute__((packed)) ist[7];

    uint32_t reserved3[2];
    uint16_t reserved4;

    /// IO map offset (should be sizeof(amd64_tss) as we don't use it). use high word
    uint16_t ioMap;
} __attribute__((packed));
}

#endif
