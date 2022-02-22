#ifndef KERNEL_PLATFORM_UEFI_ARCH_IDTTYPES_H
#define KERNEL_PLATFORM_UEFI_ARCH_IDTTYPES_H

#include <stdint.h>

namespace Platform::Amd64Uefi {
struct IdtEntry {
    /// ofset bits 0..15
    uint16_t offset1;
    /// a code segment selector in GDT/LDT
    uint16_t selector;
    /// which interrupt stack table to use, if any
    uint8_t ist;
    /// type and attributes
    uint8_t flags;
    /// offset bits 16..32
    uint16_t offset2;
    /// offset bits 33..63
    uint32_t offset3;

    // reserved bits: keep as zero
    uint32_t reserved;
} __attribute__((packed));
}

#endif
