#ifndef PLATFORM_PC_MEMMAP_H
#define PLATFORM_PC_MEMMAP_H

#include <stdint.h>

using namespace platform;

/**
 * Address of the ACPI table region. This is 0x1000000 bytes long.
 */
constexpr static const uintptr_t kPlatformRegionAcpiTables = 0xF0000000;

/**
 * Address of the platform MMIO region.
 */
constexpr static const uintptr_t kPlatformRegionMmio = 0xF1000000;

/// APIC base (0x10000 bytes)
constexpr static const uintptr_t kPlatformRegionMmioApic = kPlatformRegionMmio;
/// IOAPIC base (0x10000 bytes reserved)
constexpr static const uintptr_t kPlatformRegionMmioIoApic = kPlatformRegionMmioApic + 0x10000;

#endif
