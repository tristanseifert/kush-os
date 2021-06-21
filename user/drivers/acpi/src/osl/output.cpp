/*
 * Implementations of the ACPICA OS layer: basic IO.
 */
#include <cstdarg>
#include <cstdlib>
#include <cstdio>

#include <acpi.h>

#include "log.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"

/**
 * Prints an ACPI message to standard error.
 */
void AcpiOsVprintf(const char *format, va_list args) {
    vfprintf(stderr, format, args);
}

/**
 * Prints an ACPI message to standard error.
 */
void AcpiOsPrintf(const char *format, ...) {
    va_list va;
    va_start(va, format);

    vfprintf(stderr, format, va);

    va_end(va);
}

#pragma clang diagnostic pop

/**
 * Handles a "breakpoint" from the ACPI framework.
 *
 * We can either have the FATAL type (from the AML fatal opcode) or the BREAKPOINT type.
 */
ACPI_STATUS AcpiOsSignal(UINT32 type, void *info) {
    switch(type) {
        case ACPI_SIGNAL_FATAL: {
            auto f = reinterpret_cast<ACPI_SIGNAL_FATAL_INFO *>(info);
            Warn("ACPI fatal: type %08x, code %08x, arg %08x", f->Type, f->Code, f->Argument);
            break;
        }
        case ACPI_SIGNAL_BREAKPOINT:
            Warn("ACPI breakpoint: %s", reinterpret_cast<const char *>(info));
            break;

        default:
            Warn("AcpiOsSignal: unknown type %u", type);
            return AE_BAD_PARAMETER;
    }

    return AE_OK;
}
