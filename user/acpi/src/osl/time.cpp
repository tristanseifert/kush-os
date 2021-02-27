/*
 * Implementations of the ACPICA OS layer
 */
#include "osl.h"
#include "log.h"

#include <acpi.h>

/**
 * Returns the system timer value, in 100ns increments.
 */
UINT64 AcpiOsGetTimer() {
    // TODO: implement
    Warn("%s unimplemented", __PRETTY_FUNCTION__);
    return 0;
}
