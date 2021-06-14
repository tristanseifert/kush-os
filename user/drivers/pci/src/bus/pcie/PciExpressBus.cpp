#include "PciExpressBus.h"
#include "ConfigSpaceReader.h"
#include "Device.h"

#include "Log.h"

#include <stdexcept>
#include <system_error>

#include <sys/syscalls.h>
#include <mpack/mpack.h>
#include <driver/DrivermanClient.h>

using namespace pcie;

/// Region of virtual memory space for mapping PCIe ECAM regions
const uintptr_t PciExpressBus::kEcamMappingRange[2] = {
    // start
    0x10000000000,
    // end
    0x11000000000,
};


/**
 * Creates a PCI Express bus with the given path to its root bridge device in the forest.
 */
PciExpressBus::PciExpressBus(const std::string_view &path) : forestPath(path) {
    int err;
    auto rpc = libdriver::RpcClient::the();

    // retrieve the info
    auto ecamInfo = rpc->GetDeviceProperty(path, kEcamPropertyKey);
    if(ecamInfo.empty()) {
        throw std::runtime_error("missing PCIe resources property");
    }

    this->decodeEcamInfo(ecamInfo);
    Trace("PCIe bus %p: segment %u, busses [%u, %u], ECAM phys base $%p", this, this->segment,
            this->busses.first, this->busses.second, this->ecamPhysBase);

    // create a VM object to map this region
    const size_t numBus = 1 + (static_cast<size_t>(this->busses.second) -
            static_cast<size_t>(this->busses.first));
    const size_t ecamSize = numBus * 32 /* devices per bus */ * 
        8 /* functions per device */ *
        4096 /* bytes of config space per function */;

    err = AllocVirtualPhysRegion(this->ecamPhysBase, ecamSize,
            (VM_REGION_RW | VM_REGION_MMIO | VM_REGION_WRITETHRU), &this->ecamRegionVmHandle);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "AllocVirtualPhysRegion");
    }

    // then go and map it in the region of address space we've reserved for such shenanigans
    uintptr_t base{0};
    err = MapVirtualRegionRange(this->ecamRegionVmHandle, kEcamMappingRange, ecamSize, 0, &base);
    if(err) {
        throw std::system_error(err, std::generic_category(), "MapVirtualRegion");
    }

    this->ecamRegion = reinterpret_cast<void *>(base);

    // last, create the config reader
    this->cfgReader = new ConfigSpaceReader(this);
}

/**
 * Decodes the mpack encoded property blob provided. It is a map containing four fields: segment,
 * busMin, busMax and ecamAddr, which map onto our basic instance properties.
 */
void PciExpressBus::decodeEcamInfo(const std::span<std::byte> &range) {
    mpack_tree_t tree;
    mpack_tree_init_data(&tree, reinterpret_cast<const char *>(range.data()), range.size());
    mpack_tree_parse(&tree);
    mpack_node_t root = mpack_tree_root(&tree);

    // get the values out of it
    auto busMin = mpack_node_u8(mpack_node_map_cstr(root, "busMin"));
    auto busMax = mpack_node_u8(mpack_node_map_cstr(root, "busMax"));
    this->busses = std::make_pair(busMin, busMax);
    this->segment = mpack_node_u16(mpack_node_map_cstr(root, "segment"));
    this->ecamPhysBase = mpack_node_u64(mpack_node_map_cstr(root, "ecamAddr"));

    // clean up
    auto status = mpack_tree_destroy(&tree);
    if(status != mpack_ok) {
        Warn("%s failed: %d", "mpack_tree_destroy", status);
        return;
    }
}

/**
 * Cleans up all resources associated with the bus. It will be removed from the forest, and all
 * devices underneath will become unavailable as well.
 */
PciExpressBus::~PciExpressBus() {
    delete this->cfgReader;

    // unmap ECAM region
    if(this->ecamRegionVmHandle) {
        UnmapVirtualRegion(this->ecamRegionVmHandle);
        DeallocVirtualRegion(this->ecamRegionVmHandle);
    }
}



/**
 * Scans all devices on this bus. They will be registered with the driver manager.
 *
 * This implements a super basic brute force bus scan. It could probably be optimized to not be
 * as stupid shitty.
 */
void PciExpressBus::scan() {
    for(size_t bus = this->busses.first; bus <= this->busses.second; bus++) {
        // check each device on this bus
        for(uint8_t device = 0; device < 32; device++) {
            PciConfig::DeviceAddress addr(this->segment, bus, device);
            this->probeDevice(addr);
        }
    }
}

/**
 * Check whether there is a device at the given address on the bus.
 *
 * This relies on the fact that a value of 0xFFFF for the vendor id is invalid, and that reads from
 * nonexistent busses/devices return all ones.
 */
void PciExpressBus::probeDevice(const PciConfig::DeviceAddress &addr) {
    auto c = this->cfgReader;

    // read vendor id
    uint16_t vid = c->read(addr, 0, PciConfig::Width::Word);
    if(vid == 0xFFFF) return;

    // determine if we're dealing with a multifunction device
    uint8_t headerType = c->read(addr, 0xE, PciConfig::Width::Byte);

    this->createDeviceIfNeeded(addr);

    if(headerType & (1 << 7)) {
        // scan the remaining functions
        for(uint8_t func = 1; func < 8; func++) {
            const PciConfig::DeviceAddress fAddr(addr, func);

            // if this function is valid, probe and register
            uint16_t vid = c->read(fAddr, 0, PciConfig::Width::Word);
            if(vid == 0xFFFF) continue;

            this->createDeviceIfNeeded(fAddr);
        }
    }
}

/**
 * Creates a PCI device object for the device at the given address. It is likewise also registered
 * in with the driver manager.
 */
void PciExpressBus::createDeviceIfNeeded(const PciConfig::DeviceAddress &addr) {
    auto c = this->cfgReader;

    // read out the vendor and product IDs
    const uint32_t ids = c->read(addr, 0, PciConfig::Width::DWord);
    const uint16_t vid = (ids & 0xFFFF), pid = (ids >> 16);

    if(vid == 0xFFFF) {
        Warn("Create device at %04x:%02x:%02x:%01x, but invalid IDs: %04x:%04x", addr.segment,
                addr.bus, addr.device, addr.function, vid, pid);
        return;
    }

    // check if we've created an object here before
    if(!this->devices.contains(addr)) {
        auto device = std::make_shared<Device>(this->shared_from_this(), addr);
        this->devices.emplace(addr, std::move(device));
    }
}

