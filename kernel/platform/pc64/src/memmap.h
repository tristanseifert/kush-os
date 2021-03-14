#ifndef PLATFORM_PC_MEMMAP_H
#define PLATFORM_PC_MEMMAP_H

#include <stdint.h>

using namespace platform;

/// Platform device MMIO region start
constexpr static const uintptr_t kPlatformRegionMmio = 0xffffff0100000000;
constexpr static const size_t kPlatformRegionMmioLen = 0x100000000;

/// Temporary ACPI mapping area
constexpr static const uintptr_t kPlatformRegionAcpiTables = 0xffffff0200000000;
constexpr static const size_t kPlatformRegionAcpiTablesLen = 0x100000000;

#endif
