#include "Port.h"
#include "Controller.h"
#include "AhciRegs.h"

#include "Log.h"

#include <cstring>

#include <unistd.h>
#include <sys/syscalls.h>

/// Region of virtual memory space for AHCI port command list/receive FIS blobs
uintptr_t Port::kPrivateMappingRange[2] = {
    // start
    0x10018000000,
    // end
    0x10019000000,
};
/**
 * Creates a new AHCI port object for the given controller. This allocates all memory, initializes
 * it and configures the port and spins up any attached devices, as well as identifying them.
 */
Port::Port(Controller *controller, const uint8_t _port) : port(_port), parent(controller) {
    int err;
    //auto &regs = controller->abar->ports[_port];

    // allocate the command list and received FIS
    const size_t pageSz = sysconf(_SC_PAGESIZE);
    err = AllocVirtualAnonRegion(pageSz,
            (VM_REGION_RW | VM_REGION_WRITETHRU | VM_REGION_MMIO | VM_REGION_LOCKED),
            &this->privRegionVmHandle);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "AllocVirtualAnonRegion");
    }

    uintptr_t base{0};
    err = MapVirtualRegionRange(this->privRegionVmHandle, kPrivateMappingRange, pageSz, 0, &base);
    kPrivateMappingRange[0] += pageSz; // XXX: this seems like it shouldn't be necessary...
    if(err) {
        throw std::system_error(err, std::generic_category(), "MapVirtualRegion");
    }

    Trace("Mapped port %lu FIS/command list at $%p ($%p'h)", _port, base, this->privRegionVmHandle);

    memset(reinterpret_cast<void *>(base), 0, pageSz);

    // convert to physical and program the base registers
    uintptr_t physAddr{0};

    err = VirtualToPhysicalAddr(&base, 1, &physAddr);
    if(err) {
        throw std::system_error(err, std::generic_category(), "VirtualToPhysicalAddr");
    }

    Trace("Virt $%p -> phys $%p", base, physAddr);
}

/**
 * Cleans up resources belonging to the port.
 */
Port::~Port() {
    // shut down the port resources

    // unmap the VM object and delete it
    UnmapVirtualRegion(this->privRegionVmHandle);
    DeallocVirtualRegion(this->privRegionVmHandle);
}
