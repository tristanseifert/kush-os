#include "Controller.h"
#include "Port.h"
#include "AhciRegs.h"

#include "Log.h"

#include <sys/syscalls.h>
#include <libpci/UserClient.h>

#include <stdexcept>
#include <system_error>

/// Region of virtual memory space for mapping AHCI ABAR regions
const uintptr_t Controller::kAbarMappingRange[2] = {
    // start
    0x10000000000,
    // end
    0x10001000000,
};
/**
 * Initializes an AHCI controller attached to the given PCI device. We'll start off by mapping the
 * register areas, then perform the actual AHCI controller initialization.
 */
Controller::Controller(const std::shared_ptr<libpci::Device> &_dev) : dev(_dev) {
    int err;

    // find ABAR (this is always BAR5)
    size_t abarSize{0};
    for(const auto &a : _dev->getAddressResources()) {
        if(a.bar != libpci::Device::BaseAddress::BAR5) continue;

        // create physical region
        abarSize = a.length;
        err = AllocVirtualPhysRegion(a.base, abarSize,
                (VM_REGION_RW | VM_REGION_MMIO | VM_REGION_WRITETHRU), &this->abarVmHandle);
        if(err) {
            throw std::system_error(errno, std::generic_category(), "AllocVirtualPhysRegion");
        }
    }

    if(!this->abarVmHandle) {
        throw std::runtime_error("Failed to find ABAR");
    }

    // map ABAR
    uintptr_t base{0};
    err = MapVirtualRegionRange(this->abarVmHandle, kAbarMappingRange, abarSize, 0, &base);
    if(err) {
        throw std::system_error(err, std::generic_category(), "MapVirtualRegion");
    }

    this->abar = reinterpret_cast<volatile AhciHbaRegisters *>(base);

    // TODO: verify version?
    Trace("AHCI version for %s: %08x", _dev->getPath().c_str(), this->abar->version);

    // grab ownership from BIOS/system firmware if needed and reset the HBA
    if(this->abar->hostCapsExt & AhciHostCaps2::BiosHandoffSupported) {
        this->performBiosHandoff();
    }

    this->reset();

    const auto hostCaps = this->abar->hostCaps;
    this->supports64Bit = hostCaps & AhciHostCaps::Supports64Bit;
    this->supportsNCQ = hostCaps & AhciHostCaps::SataNCQ;
    this->supportsSataNotifications = hostCaps & AhciHostCaps::SNotification;
    this->supportsStaggeredSpinup = hostCaps & AhciHostCaps::StaggeredSpinup;

    this->sataGen = (hostCaps & (AhciHostCaps::HbaMaxSpeedMask)) >> AhciHostCaps::HbaMaxSpeedOffset;

    // put the controller into AHCI mode and read some info
    this->abar->ghc = this->abar->ghc | AhciGhc::AhciEnable;
    this->validPorts = this->abar->portsImplemented;

    // configure each of the implemented ports
    for(size_t i = 0; i < kMaxPorts; i++) {
        if(!(this->abar->portsImplemented & (1 << i))) continue;
        auto port = std::make_shared<Port>(this, i);

        this->ports[i] = std::move(port);
    }
}

/**
 * Performs the BIOS/OS handoff procedure. It is described in detail in section 10.6 of the AHCI
 * specification (version 1.3).
 *
 * XXX: This is currently untested
 */
void Controller::performBiosHandoff() {
    // 1. set OS ownership flag
    this->abar->bohc = this->abar->bohc | AhciBohc::OsOwnershipFlag;
    // 3. wait for BIOS ownership flag to be cleared
    while(this->abar->bohc & AhciBohc::BiosOwnershipFlag) {}
    // 4. give HBA 25msec to set the busy flag
    ThreadUsleep(25 * 1000);
    const bool needsCleanup = this->abar->bohc & AhciBohc::BiosBusy;

    // 5. if the busy flag was set, wait at least 2 seconds for commands to complete
    if(needsCleanup) {
        ThreadUsleep(2 * 1000 * 1000);
    }
}

/**
 * Resets the HBA.
 *
 * TODO: We need to add a 1 second timeout; if the HBA doesn't reset in that timeframe, we should
 * give up and assume it's fucked.
 */
void Controller::reset() {
    this->abar->ghc = this->abar->ghc | AhciGhc::Reset;
    while(this->abar->ghc & AhciGhc::Reset) {}
}



/**
 * Cleans up the resources allocated by the AHCI controller.
 */
Controller::~Controller() {
    // lastly, remove the ABAR mapping
    if(this->abarVmHandle) {
        UnmapVirtualRegion(this->abarVmHandle);
        DeallocVirtualRegion(this->abarVmHandle);
    }
}
