/*
 * Implementations of the ACPICA OS layer
 */
#include "osl.h"
#include "log.h"

#include <cstdlib>
#include <acpi.h>


/**
 * The system is about to enter a sleep mode.
 */
ACPI_STATUS AcpiOsEnterSleep(UINT8 SleepState, UINT32 RegaValue, UINT32 RegbValue) {
    Info("AcpiOsEnterSleep state %u, rega %08x regb %08x", SleepState, RegaValue, RegbValue);
    return AE_OK;
}
