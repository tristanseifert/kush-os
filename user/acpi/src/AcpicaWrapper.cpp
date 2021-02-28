#include "AcpicaWrapper.h"
#include "log.h"

#include <cassert>
#include <cstddef>
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
        Abort("AcpiInitializeSubsystem failed: %s", AcpiFormatException(status));
    }

    // read the tables
    status = AcpiInitializeTables(nullptr, 16, true);
    if(ACPI_FAILURE(status)) {
        Abort("AcpiInitializeTables failed: %s", AcpiFormatException(status));
    }

   // Install the default address space handlers.
    status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
        ACPI_ADR_SPACE_SYSTEM_MEMORY, ACPI_DEFAULT_HANDLER, NULL, NULL);
    if(ACPI_FAILURE(status)) {
        Warn("Could not initialise SystemMemory handler, %s!", 
        AcpiFormatException(status));
    }

    status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
    ACPI_ADR_SPACE_SYSTEM_IO, ACPI_DEFAULT_HANDLER, NULL, NULL);
    if(ACPI_FAILURE(status)) {
        Warn("Could not initialise SystemIO handler, %s!",
        AcpiFormatException(status));
    }

    status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
    ACPI_ADR_SPACE_PCI_CONFIG, ACPI_DEFAULT_HANDLER, NULL, NULL);
    if(ACPI_FAILURE(status)) {
        Warn("Could not initialise PciConfig handler, %s!",
        AcpiFormatException(status));
    }


    // create ACPI namespace
    Info("Loading ACPI tables");

    status = AcpiLoadTables();
    if(ACPI_FAILURE(status)) {
        Abort("AcpiLoadTables failed: %s", AcpiFormatException(status));
    }

    // initialize ACPI hardware
    Info("Enabling ACPI");

    status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if(ACPI_FAILURE(status)) {
        Abort("AcpiEnableSubsystem failed: %s", AcpiFormatException(status));
    }

    // install handlers
    this->installHandlers();

    // finish namespace initialization
    //AcpiDbgLevel = ACPI_DEBUG_DEFAULT | ACPI_LV_VERBOSITY1;
    Info("Initializing ACPI objects");

    status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if(ACPI_FAILURE(status)) {
        Abort("AcpiInitializeObjects failed: %s", AcpiFormatException(status));
    }

    // run _OSC on the root object

    // configure APIC irq routing
    this->configureApic();

    // done
    Success("ACPICA initialized");
}

/**
 * Install ACPICA event handlers.
 */
void AcpicaWrapper::installHandlers() {

}

/**
 * Configure the hardware to use IOAPIC interrupts rather than legacy PIC interrupts.
 */
void AcpicaWrapper::configureApic() {
    ACPI_OBJECT arg1;
    ACPI_OBJECT_LIST args;

    arg1.Type = ACPI_TYPE_INTEGER;
    arg1.Integer.Value = 1;            /* 0 - PIC, 1 - IOAPIC */
    args.Count = 1;
    args.Pointer = &arg1;

    auto status = AcpiEvaluateObject(ACPI_ROOT_OBJECT, "_PIC", &args, nullptr);

    if(!ACPI_SUCCESS(status)) {
        if(status == AE_NOT_FOUND) {
            Warn("No _PIC method");
        } else {
            Abort("failed to set IRQ controller mode: %s", AcpiFormatException(status));
        }
    }
}



/**
 * Enumerates all PCI busses in the ACPI namespace, and then launches the PCI driver for them.
 *
 * PNP0A03 = PCI bus
 * PNP0A05 = Generic ACPI bus
 * PNP0A06 = Generic ACPI extended IO bus
 * PNP0A08 = PCI express bus
 */
void AcpicaWrapper::probePci() {
    void *retval;
    ACPI_STATUS status = AcpiGetDevices("PNP0A03",
            [](ACPI_HANDLE obj, UINT32 level, void *ctx, void **ret) -> ACPI_STATUS {
        auto acpica = reinterpret_cast<AcpicaWrapper *>(ctx);

        // get some object info
        ACPI_DEVICE_INFO *info;
        auto status = AcpiGetObjectInfo(obj, &info);

        if(status != AE_OK) {
            Warn("AcpiGetObjectInfo failed: %s", AcpiFormatException(status));
            return 0;
        }

        ACPI_FREE(info);

        // process it if it's a root bridge
        if(info->Flags & ACPI_PCI_ROOT_BRIDGE) {
            acpica->foundPciRoot(obj);
        }

        // continue enumeration
        return 0;
    }, this, &retval);
    if (status != AE_OK) {
        Abort("AcpiGetDevices failed to enumerate PCI busses: %s", AcpiFormatException(status));
    }
}

/**
 * Processes a found root bridge.
 */
void AcpicaWrapper::foundPciRoot(ACPI_HANDLE object) {
    ACPI_STATUS status;
    ACPI_OBJECT ret1;
    ACPI_BUFFER retBuf;

    // get its name
    ACPI_BUFFER buffer;
    char name[128];

    buffer.Length = sizeof(name);
    buffer.Pointer = name;

    status = AcpiGetName(object, ACPI_FULL_PATHNAME, &buffer);
    if(status == AE_OK) {
        Trace("PCI root bridge: %s", name);
    }

    // initialize buffer
    ret1.Type = ACPI_TYPE_INTEGER;
    retBuf.Length = sizeof(ret1);
    retBuf.Pointer = &ret1;

    // find its address (high word = device, low word = function)
    uint32_t addr = 0;

    status = AcpiEvaluateObjectTyped(object, "_ADR", nullptr, &retBuf, ACPI_TYPE_INTEGER);
    if(status != AE_OK) {
        Abort("Failed to evaluate %s on %s: %s", "_ADR", name, AcpiFormatException(status));
    } else {
        addr = ret1.Integer.Value;
    }

    // get its base bus number
    uint32_t bus = 0;

    status = AcpiEvaluateObjectTyped(object, "_BBN", nullptr, &retBuf, ACPI_TYPE_INTEGER);
    if(status != AE_OK) {
        Warn("Failed to evaluate %s on %s: %s", "_BBN", name, AcpiFormatException(status));
    } else {
        addr = ret1.Integer.Value;
    } 

    // get its bus segment
    uint32_t seg = 0;

    status = AcpiEvaluateObjectTyped(object, "_SEG", nullptr, &retBuf, ACPI_TYPE_INTEGER);
    if(status != AE_OK) {
        seg = 0; // assume segment as 0 if method is unavailable
        Warn("Failed to evaluate %s on %s: %s", "_SEG", name, AcpiFormatException(status));
    } else {
        addr = ret1.Integer.Value;
    } 

    Trace("Bridge %s: address %08x bus %u segment %u", name, addr, bus, seg);

    // get the interrupt routing for it
    this->pciGetIrqRoutes(object);
}

/**
 * Reads out the PCI interrupt routing tables.
 */
void AcpicaWrapper::pciGetIrqRoutes(ACPI_HANDLE object) {
    ACPI_STATUS status;

    // set up the buffer
    ACPI_BUFFER buf;
    buf.Length = ACPI_ALLOCATE_BUFFER;
    buf.Pointer = nullptr;

    // read tables
    status = AcpiGetIrqRoutingTable(object, &buf);
    if(status != AE_OK) {
        Abort("AcpiGetIrqRoutingTable failed: %s", AcpiFormatException(status));
    }

    // iterate over the interrupts
    std::byte *scan = reinterpret_cast<std::byte *>(buf.Pointer);

    while(true) {
        // get table and exit if we've reached the end
        auto table = reinterpret_cast<const ACPI_PCI_ROUTING_TABLE *>(scan);
        if(!table->Length) {
            break;
        }

        uint8_t slot = (table->Address >> 16);

        // static interrupt assignment
        if(!table->Source[0]) {
            Trace("gsi %u pin %u (slot %u)", table->SourceIndex, table->Pin, slot);
        }
        // the assignment is dynamic
        else {
            // get PCI link
            ACPI_HANDLE linkObject;
            status = AcpiGetHandle(object, table->Source, &linkObject);
            if(status != AE_OK) {
                Abort("failed to get source '%s': %s", table->Source, AcpiFormatException(status));
            }

            // get associated IRQ
            ACPI_BUFFER resbuf;
            resbuf.Length = ACPI_ALLOCATE_BUFFER;
            resbuf.Pointer = NULL;

            status = AcpiGetCurrentResources(linkObject, &resbuf);
            if(status != AE_OK) {
                Abort("AcpiGetCurrentResources failed for '%s': %s", table->Source, AcpiFormatException(status));
            }

            auto rscan = reinterpret_cast<std::byte *>(resbuf.Pointer);
            int devIRQ = -1;
            while(1) {
                auto res = reinterpret_cast<ACPI_RESOURCE *>(rscan);

                if(res->Type == ACPI_RESOURCE_TYPE_END_TAG) {
                    break;
                }

                // XXX: is this code right?
                else if(res->Type == ACPI_RESOURCE_TYPE_IRQ) {
                    devIRQ = res->Data.Irq.Interrupts[table->SourceIndex];
                    break;
                }
                else if(res->Type == ACPI_RESOURCE_TYPE_EXTENDED_IRQ) {
                    const auto &ext = res->Data.ExtendedIrq;
                    devIRQ = ext.Interrupts[table->SourceIndex];
                    break;
                }

                // check next resource
                rscan += res->Length;
            }

            // release resource buffer
            free(resbuf.Pointer);

            // record the interrupt we got
            if(devIRQ == -1) {
                Abort("failed to derive IRQ for device %d from '%s'", (int)slot, table->Source);
            }

            Trace("DEVICE %d intpin INT%c# -> IRQ %d (from '%s')", (int) slot, 'A'+table->Pin, devIRQ, table->Source);
        }

        // go to next
        scan += table->Length;
    }

    // clean up
    free(buf.Pointer);
}
