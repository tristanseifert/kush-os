/*
 * Implementations of the ACPICA OS layer
 */
#include "osl.h"
#include <cstdlib>

#include <acpi.h>

using namespace osl;

/**
 * Initializes the OS layer.
 */
ACPI_STATUS AcpiOsInitialize(void) {
    InitIrqDispatcher();
    InitPciConfig();

    return AE_OK;
}

/**
 * Terminates the OS layer.
 */
ACPI_STATUS AcpiOsTerminate(void) {
    StopIrqDispatcher();

    return AE_OK;
}
