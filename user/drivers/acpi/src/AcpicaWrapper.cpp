#include "AcpicaWrapper.h"
#include "AcpiUtils.h"
#include "log.h"

#include "bus/PciBus.h"
#include "bus/PciExpressBus.h"
#include "bus/Ps2Bus.h"

#include <cassert>
#include <cstddef>
#include <acpi.h>

using namespace acpi;

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

    AcpiDbgLevel = ACPI_NORMAL_DEFAULT;

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

    auto status = AcpiEvaluateObject(nullptr, "\\_PIC", &args, nullptr);

    if(!ACPI_SUCCESS(status)) {
        if(status == AE_NOT_FOUND) {
            Warn("No _PIC method");
        } else {
            Abort("failed to set IRQ controller mode: %s", AcpiFormatException(status));
        }
    }
}

/**
 * Gets information on all busses detected in the ACPI tables.
 */
void AcpicaWrapper::probeBusses() {
    // probe the busses
#ifndef __amd64__
    gShared->probePci();
#endif
    gShared->probePciExpress();
    gShared->probePcDevices();

    // yeet
    for(const auto &[id, bus] : gShared->busses) {
        // log the loaded busses
        if(gLogBusses) {
            Trace("Discovered bus %u:%s at %s: %p", id, bus->getName().c_str(), bus->getAcpiPath().c_str(), bus.get());
        }

        // load its driver
        bus->loadDriver(id);
    }
}

/**
 * Finds all platform devices in the ACPI.
 */
void AcpicaWrapper::probeDevices() {
    // nothing... yet
}



/**
 * Enumerates all PCI busses in the ACPI namespace, and then launches the PCI driver for them.
 *
 * PNP0A03 = PCI bus
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
    const auto name = AcpiUtils::getName(object);

    // initialize buffer
    ret1.Type = ACPI_TYPE_INTEGER;
    retBuf.Length = sizeof(ret1);
    retBuf.Pointer = &ret1;

    // find its address (high word = device, low word = function)
    uint32_t addr = 0;

    status = AcpiEvaluateObjectTyped(object, "_ADR", nullptr, &retBuf, ACPI_TYPE_INTEGER);
    if(status != AE_OK) {
        Abort("Failed to evaluate %s on %s: %s", "_ADR", name.c_str(), AcpiFormatException(status));
    } else {
        addr = ret1.Integer.Value;
    }

    // get its base bus number
    uint32_t bus = 0;

    status = AcpiEvaluateObjectTyped(object, "_BBN", nullptr, &retBuf, ACPI_TYPE_INTEGER);
    if(status != AE_OK) {
        Warn("Failed to evaluate %s on %s: %s", "_BBN", name.c_str(), AcpiFormatException(status));
    } else {
        addr = ret1.Integer.Value;
    } 

    // get its bus segment
    uint32_t seg = 0;

    status = AcpiEvaluateObjectTyped(object, "_SEG", nullptr, &retBuf, ACPI_TYPE_INTEGER);
    if(status != AE_OK) {
        seg = 0; // assume segment as 0 if method is unavailable
        Warn("Failed to evaluate %s on %s: %s", "_SEG", name.c_str(), AcpiFormatException(status));
    } else {
        addr = ret1.Integer.Value;
    } 

    if(gLogBusses) {
        Trace("Bridge %s: address %08x bus %u segment %u", name.c_str(), addr, bus, seg);
    }

    // create the bus
    auto bobj = std::make_shared<PciBus>(nullptr, name, bus, addr, seg);
    bobj->getIrqRoutes(object);

    // store bus
    this->busses.emplace(this->nextBusId++, std::dynamic_pointer_cast<Bus>(bobj));
}

/**
 * Parses the MCFG table to find all PCIe configuration regions
 */
void AcpicaWrapper::probePciExpress() {
    // try to locate the table
    ACPI_TABLE_HEADER *hdr{nullptr};
    ACPI_STATUS ret = AcpiGetTable(ACPI_SIG_MCFG, 1, &hdr);
    if(ret == AE_NOT_FOUND) {
        Warn("Failed to find MCFG table (%d) - does this machine have a PCIe bus?", ret);
        return;
    } else if(ret != AE_OK) {
        Abort("AcpiGetTable() failed: %d", ret);
    }

    const auto mcfg = reinterpret_cast<ACPI_TABLE_MCFG *>(hdr);
    const auto numEntries = (hdr->Length - sizeof(*mcfg)) / sizeof(ACPI_MCFG_ALLOCATION);

    if(gLogBusses) Trace("MCFG table is at %p; has %lu entries", mcfg, numEntries);

    // handle each entry
    const auto entries = reinterpret_cast<ACPI_MCFG_ALLOCATION *>(
            reinterpret_cast<uintptr_t>(mcfg) + sizeof(*mcfg));

    for(size_t i = 0; i < numEntries; i++) {
        const auto &e = entries[i];
        this->foundPcieSegment(i, e);
    }
}

/**
 * Processes a found PCI Express segment.
 */
void AcpicaWrapper::foundPcieSegment(const size_t idx, const ACPI_MCFG_ALLOCATION &s) {
    auto pcie = std::make_shared<PciExpressBus>(nullptr, "", s);
    this->busses.emplace(this->nextBusId++, std::dynamic_pointer_cast<Bus>(pcie));
}




/**
 * Search for devices commonly found on a PC, including:
 *
 * - PS/2 keyboard and mouse controller
 */
void AcpicaWrapper::probePcDevices() {
    // test for a PS/2 keyboard/mouse controller
    auto ps2 = Ps2Bus::probe();

    if(ps2) {
        this->busses.emplace(this->nextBusId++, std::dynamic_pointer_cast<Bus>(ps2));
    }
}

