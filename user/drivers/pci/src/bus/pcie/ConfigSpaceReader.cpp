#include "ConfigSpaceReader.h"
#include "PciExpressBus.h"

#include <cstring>
#include <stdexcept>

using namespace pcie;

/**
 * Creates a config space reader for the given bus.
 */
ConfigSpaceReader::ConfigSpaceReader(PciExpressBus *_bus) : bus(_bus) {
}

/**
 * Ensures the given device address is on the bus this reader is responsible for.
 */
void ConfigSpaceReader::EnsureOnBus(PciExpressBus *bus, const DeviceAddress &addr) {
    // segment must be the same
    if(addr.segment != bus->segment) {
        throw std::runtime_error("invalid bus segment");
    }
    // validate the bus is valid
    else if(addr.bus < bus->busses.first || addr.bus > bus->busses.second) {
        throw std::runtime_error("bus out of range");
    }
    // validate device/function
    else if(addr.device > 31) {
        throw std::runtime_error("invalid device");
    }
    else if(addr.function > 7) {
        throw std::runtime_error("invalid function");
    }
}

/**
 * Gets the virtual address of the part of the ECAM Region that maps the given device's register.
 *
 * @note This assumes the device address passed in has been validated.
 *
 * @return Address, or `nullptr` if offet is out of bounds.
 */
void *ConfigSpaceReader::GetEcamAddr(PciExpressBus *bus, const DeviceAddress &addr,
        const size_t reg, const Width width) {
    // base into ECAM space
    uintptr_t deviceCfgStart = reinterpret_cast<uintptr_t>(bus->ecamRegion);
    deviceCfgStart += ((addr.bus - bus->busses.first) << 20 | addr.device << 15 | addr.function << 12);

    switch(width) {
        case Width::Byte:
            if(reg > 4095) throw std::runtime_error("register offset out of range");
            break;
        case Width::Word:
            if(reg > 4094) throw std::runtime_error("register offset out of range");
            break;
        case Width::DWord:
            if(reg > 4092) throw std::runtime_error("register offset out of range");
            break;
        case Width::QWord:
            if(reg > 4088) throw std::runtime_error("register offset out of range");
            break;
    }

    // if we get here, the register offset is valid
    return reinterpret_cast<void *>(deviceCfgStart + reg);
}



/**
 * Perform a read of PCI configuration space.
 */
uint64_t ConfigSpaceReader::read(const DeviceAddress &device, const size_t reg,
        const Width width) {
    // validate args and get the memory address of the register
    EnsureOnBus(this->bus, device);

    auto addr = GetEcamAddr(this->bus, device, reg, width);
    if(!addr) throw std::runtime_error("invalid register offset");

    // perform the access
    switch(width) {
        case Width::Byte: {
            uint8_t temp;
            memcpy(&temp, addr, sizeof(temp));
            return temp;
        }
        case Width::Word: {
            uint16_t temp;
            memcpy(&temp, addr, sizeof(temp));
            return temp;
        }
        case Width::DWord: {
            uint32_t temp;
            memcpy(&temp, addr, sizeof(temp));
            return temp;
        }
        case Width::QWord: {
            uint64_t temp;
            memcpy(&temp, addr, sizeof(temp));
            return temp;
        }
    }
}

/**
 * Performs a write to PCI configuration space.
 */
void ConfigSpaceReader::write(const DeviceAddress &device, const size_t reg, const Width width,
        const uint64_t value) {
    // validate args and get the memory address of the register
    EnsureOnBus(this->bus, device);

    auto addr = GetEcamAddr(this->bus, device, reg, width);
    if(!addr) throw std::runtime_error("invalid register offset");

    // perform the access
    switch(width) {
        case Width::Byte: {
            uint8_t temp{static_cast<uint8_t>(value)};
            memcpy(addr, &temp, sizeof(temp));
            break;
        }
        case Width::Word: {
            uint16_t temp{static_cast<uint16_t>(value)};
            memcpy(addr, &temp, sizeof(temp));
            break;
        }
        case Width::DWord: {
            uint32_t temp{static_cast<uint32_t>(value)};
            memcpy(addr, &temp, sizeof(temp));
            break;
        }
        case Width::QWord: {
            memcpy(addr, &value, sizeof(value));
            break;
        }
    }
}

