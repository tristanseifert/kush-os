#include "Server.h"
#include "BusRegistry.h"
#include "Log.h"

#include "bus/pcie/PciExpressBus.h"
#include "bus/pcie/Device.h"

#include <rpc/rt/ServerPortRpcStream.h>

RpcServer *RpcServer::gShared{nullptr};

/**
 * Initializes the global RPC server instance. This will create the appropriate listening IO
 * object and registers the port.
 */
void RpcServer::init() {
    auto strm = std::make_shared<rpc::rt::ServerPortRpcStream>(kRpcEndpointName);
    gShared = new RpcServer(strm);
}

/**
 * Checks if we have a device for the given address.
 *
 * @return Forest path to the device, or empty string if there is no such device.
 */
std::string RpcServer::implGetDeviceAt(const libdriver::pci::BusAddress &address) {
    // get the bus and test if it contains the device
    auto bus = BusRegistry::the()->get(address);
    if(!bus) return "";

    if(!bus->hasDevice(address)) return "";
    auto device = bus->getDevice(address);

    // return info about the device
    return device->getForestPath();
}

/**
 * Performs a 32-bit wide read from the device's config space.
 */
uint32_t RpcServer::implReadCfgSpace32(const libdriver::pci::BusAddress &address, uint16_t offset) {
    if(kLogCfgRead) {
        Trace("Cfg space read: %04x:%02x:%02x:%02x off $%03x", address.segment, address.bus,
                address.device, address.function, offset);
    }

    auto bus = BusRegistry::the()->get(address);
    if(!bus) return 0;
    auto cfg = bus->getConfigIo();

    return cfg->read(address, offset, PciConfig::Width::DWord);
}

/**
 * Performs a 32-bit wide write to the device's config space.
 */
void RpcServer::implWriteCfgSpace32(const libdriver::pci::BusAddress &address, uint16_t offset,
        uint32_t value) {
    if(kLogCfgWrite) {
        Trace("Cfg space write: %04x:%02x:%02x:%02x off $%03x => %08x", address.segment, address.bus,
            address.device, address.function, offset, value);
    }

    Warn("TODO: implement %s", __PRETTY_FUNCTION__);
}

