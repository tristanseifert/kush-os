#include "AcpicaWrapper.h"
#include "log.h"

#include <cassert>
#include <acpi.h>

/// Shared ACPICA wrapper
AcpicaWrapper *AcpicaWrapper::gShared = nullptr;

/**
 * Initialize the shared ACPICA handler.
 */
void AcpicaWrapper::init() {
    assert(!gShared);
    gShared = new AcpicaWrapper;
}

/**
 * Initializes ACPICA.
 */
AcpicaWrapper::AcpicaWrapper() {
    ACPI_STATUS status;

    // debug all the things
    // AcpiDbgLevel = ACPI_DEBUG_ALL;
    AcpiDbgLayer = ACPI_ALL_COMPONENTS;

    // initialize ACPICA subsystem
    status = AcpiInitializeSubsystem();
    if(ACPI_FAILURE(status)) {
        Abort("AcpiInitializeSubsystem failed: %d", status);
    }

    // read the tables
    status = AcpiInitializeTables(nullptr, 16, true);
    if(ACPI_FAILURE(status)) {
        Abort("AcpiInitializeTables failed: %d", status);
    }

    // create ACPI namespace
    Info("Loading ACPI tables");

    status = AcpiLoadTables();
    if(ACPI_FAILURE(status)) {
        Abort("AcpiLoadTables failed: %d", status);
    }

    // initialize ACPI hardware
    Info("Enabling ACPI");

    status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if(ACPI_FAILURE(status)) {
        Abort("AcpiEnableSubsystem failed: %d", status);
    }

    // install handlers
    this->installHandlers();

    // finish namespace initialization
    //AcpiDbgLevel = ACPI_DEBUG_DEFAULT | ACPI_LV_VERBOSITY1;
    Info("Initializing ACPI objects");

    status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if(ACPI_FAILURE(status)) {
        Abort("AcpiInitializeObjects failed: %d", status);
    }

    // done
    Success("ACPICA initialized");
}

/**
 * Install ACPICA event handlers.
 */
void AcpicaWrapper::installHandlers() {

}
