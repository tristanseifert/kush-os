#include "Client.h"
#include "Helpers.h"

#include <driver/DrivermanClient.h>
#include <driver/PciUserClientTypes.h>

#include <stdexcept>

using namespace libdriver::pci;

/**
 * Initializes a device based on a given bus address.
 *
 * We'll translate the address to a forest path, and if this succeeds, assume the device exists and
 * use the address as is.
 */
Device::Device(const BusAddress &addr) : address(addr) {
    auto rpc = UserClient::the();
    const auto path = rpc->GetDeviceAt(addr);
    if(path.empty()) {
        throw std::invalid_argument("Invalid PCIe address");
    }

    this->path = path;

    this->probeConfigSpace();
}

/**
 * Initializes a device based on its forest path. We'll read out the PCI info property from it to
 * decode the device address.
 */
Device::Device(const std::string_view &_path) : path(_path) {
    auto driverman = libdriver::RpcClient::the();

    // read the property out
    auto value = driverman->GetDeviceProperty(_path, kPciExpressInfoPropertyName);
    if(value.empty()) {
        throw std::invalid_argument("Path does not exist or is not a valid PCIe device");
    }

    if(!internal::DecodeAddressInfo(value, this->address)) {
        throw std::runtime_error("Failed to decode PCIe address info");
    }

    this->probeConfigSpace();
}

/**
 * Reads the vendor/product ids, class identifiers and some other information from the device's
 * configuration space.
 */
void Device::probeConfigSpace() {
    // read vid/pid
    this->vid = this->readCfg16(0x00);
    this->pid = this->readCfg16(0x02);

    // read (sub) class identifiers
    this->classId = this->readCfg8(0xB);
    this->subclassId = this->readCfg8(0xA);
}

/**
 * Performs a 32-bit read from the device's config space.
 */
uint32_t Device::readCfg32(const size_t index) const {
    return UserClient::the()->ReadCfgSpace32(this->address, index);
}

/**
 * Performs a 32-bit write to the device's config space.
 */
void Device::writeCfg32(const size_t index, const uint32_t value) {
    return UserClient::the()->WriteCfgSpace32(this->address, index, value);
}
