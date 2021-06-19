#include "Device.h"
#include "Helpers.h"
#include "userclient/Client.h"

#include <driver/DrivermanClient.h>

#include <cstdio>
#include <stdexcept>

using namespace libpci;

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
    this->classId = this->readCfg8(0x0B);
    this->subclassId = this->readCfg8(0x0A);

    this->headerType = this->readCfg8(0x0E) & 0x7F;

    // check if we have a capabilities list to follow
    const auto status = this->readCfg16(0x06);
    if(status & (1 << 4)) {
        this->readCapabilities();
    }

    this->readAddrRegions();
}

/**
 * Reads the standard PCI capabilities list.
 */
void Device::readCapabilities() {
    // get the index of the first capability; it is stored differently based on the header type
    size_t capOffsetIdx{0x34};
    if(this->headerType == 0x02) { // PCI to CardBus bridge
        capOffsetIdx = 0x14;
    }

    size_t offset{this->readCfg8(capOffsetIdx)};

    while(offset) {
        // read the capability header
        const uint32_t header{this->readCfg32(offset)};
        const uint8_t id = header & 0xFF,
                    next = (header & 0xFF00) >> 8;

        // create capability object
        Capability c{id, static_cast<uint16_t>(offset)};
        this->capabilities.emplace_back(std::move(c));

        offset = next;
    }
}

/**
 * Reads the PCIe extended capabilities list. The first entry will _always_ be at address $100 in 
 * the PCIe configuration space.
 *
 * Capabilities have a very weird header, where bits 31-20 are the "next ptr", then bits 19 to 16
 * consist of the version of the capability, followed by a 16-bit capability ID.
 */
void Device::readExtendedCapabilities() {
    size_t offset{0x100};

    while(offset) {
        // read the header word and extract the values
        const uint32_t header{this->readCfg32(offset)};
        const uint16_t next = (header & 0xFFF00000) >> 20;
        const uint8_t version = (header & 0xF0000) >> 16;
        const uint16_t id = header & 0xFFFF;

        // insert the capability and prepare for next
        Capability c{id, static_cast<uint16_t>(offset), version};
        this->capabilities.emplace_back(std::move(c));
        offset = next;
    }
}

/**
 * Reads the base address registers of the PCI device.
 */
void Device::readAddrRegions() {
    size_t numBars{6}, barStart{0x10};

    if(this->headerType == 0x01) {
        numBars = 2;
    } else if(this->headerType == 0x02) return;

    std::vector<AddressResource> newBars;

    static const BaseAddress kBarMap[] = {
        BaseAddress::BAR0, BaseAddress::BAR1, BaseAddress::BAR2,
        BaseAddress::BAR3, BaseAddress::BAR4, BaseAddress::BAR5,
    };

    // read each of the BARs
    for(size_t i = 0; i < numBars; i++) {
        const size_t barOff{barStart + (i * 4)};
        const auto bar = this->readCfg32(barOff);

        // if the BAR is empty, we can ignore it
        if(!bar) continue;

        const auto barId{kBarMap[i]};

        // size this BAR
        this->writeCfg32(barOff, ~0);
        uint32_t size = this->readCfg32(barOff);
        this->writeCfg32(barOff, bar);

        // have we got an IO space BAR?
        if(bar & 1) {
            size &= ~0x3;
            size = (~size) + 1;

            const auto base = bar & ~0x3;

            newBars.push_back(AddressResource(barId, base, size));
        }
        // otherwise, it is a memory space BAR
        else {
            size &= ~0xF;
            size = (~size) + 1;

            const uint8_t type = (bar & 0b110) >> 1;
            const bool prefetchable = (bar & (1 << 3));
            const auto base = bar & ~0xF;

            if(type != 0x00) {
                fprintf(stderr, "BAR%lu: memory, base $%p, prefetchable %d !! unknown type %02x, size $%x\n", i, base, prefetchable, type, size);
            } else {
                newBars.push_back(AddressResource(barId, base, size, prefetchable, false));
            }
        }
    }

    this->bars = newBars;
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



/**
 * Enables message signaled interrupts.
 *
 * @param cpu APIC ID of the processor to target with interrupts
 * @param vector Vector to fire on the target processor
 * @param numVectors Total number of MSI vectors to install (powers of 2 between 1 and 32)
 *
 * TODO: numVectors is ignored and defaults to 1
 *
 * TODO: This is very amd64 specific. Should it go elsewhere?
 */
void Device::enableMsi(const uintptr_t cpu, const uintptr_t vector, const size_t numVectors) {
    const auto &cap = this->getMsiCap();
    const auto base = cap.offset;

    // read its config
    uint32_t temp = this->readCfg32(base);
    const bool is64Bit = (temp & (1 << 23));

    // configure the message address and data
    this->writeCfg32(base+4, (0xFEE00000 | (cpu << 12)));
    if(is64Bit) {
        this->writeCfg32(base+0x8, 0);
    }

    uint32_t msgData = vector & 0xFF; // (0 << 15) for edge trigger
    if(is64Bit) {
        this->writeCfg32(base+0xC, msgData);
    } else {
        this->writeCfg32(base+0x8, msgData);
    }

    // last, enable the interrupshione
    temp &= ~(0b111 << 20);
    temp |= (1 << 16);

    this->writeCfg32(base, temp);
}

/**
 * Disables message signaled interrupts.
 */
void Device::disableMsi() {
    const auto &cap = this->getMsiCap();

    // clear the enable bit in the MSI config space
    uint32_t temp = this->readCfg32(cap.offset);
    temp &= ~(1 << 16);
    this->writeCfg32(cap.offset, temp);
}
