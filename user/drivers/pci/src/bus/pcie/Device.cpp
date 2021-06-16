#include "Device.h"
#include "PciExpressBus.h"

#include "Log.h"
#include "bus/PciConfig.h"

#include <fmt/core.h>
#include <mpack/mpack.h>
#include <driver/DrivermanClient.h>

using namespace pcie;

/**
 * Initializes the device object. This reads some information from the configuration space of the
 * device, then registers it with the forest.
 */
Device::Device(const BusPtr &_bus, const DeviceAddress &_address) : bus(_bus), address(_address) {
    auto c = _bus->getConfigIo();

    // read some device infos
    const uint32_t ids = c->read(this->address, 0, PciConfig::Width::DWord);
    const uint16_t vid = (ids & 0xFFFF), pid = (ids >> 16);
    //const uint16_t classes = c->read(this->address, 0xA, PciConfig::Width::Word);
    //const uint8_t subclass = (classes & 0xFF), classCode = (classes >> 8);

    // build the string to advertise under
    std::string name = fmt::format("PciExpress{:04x}.{:04x}@{:x}.{:x}.{:x},GenericPciExpressDevice",
            vid, pid, _address.bus, _address.device, _address.function);

    // advertise the device and set its information
    auto rpc = libdriver::RpcClient::the();

    std::vector<std::byte> aux;
    this->serializeAuxData(aux);

    this->path = rpc->AddDevice(_bus->getForestPath(), name);
    if(kLogPaths) Trace("%s", fmt::format("PCI device at {} registered as {}", _address,
                this->path).c_str());

    rpc->SetDeviceProperty(this->path, kPciAddressPropertyName, aux);
}

/**
 * Remove the device from the forest when deallocating.
 */
Device::~Device() {
    // TODO
}



/**
 * Serializes information about this device.
 */
void Device::serializeAuxData(std::vector<std::byte> &out) {
    char *data;
    size_t size;

    const auto &a = this->address;

    // set up writer
    mpack_writer_t writer;
    mpack_writer_init_growable(&writer, &data, &size);

    mpack_start_map(&writer, 4);
    mpack_write_cstr(&writer, "segment");
    mpack_write_u16(&writer, a.segment);
    mpack_write_cstr(&writer, "bus");
    mpack_write_u8(&writer, a.bus);
    mpack_write_cstr(&writer, "device");
    mpack_write_u8(&writer, a.device);
    mpack_write_cstr(&writer, "function");
    mpack_write_u8(&writer, a.function);

    // clean up
    mpack_finish_map(&writer);

    auto status = mpack_writer_destroy(&writer);
    if(status != mpack_ok) {
        Warn("%s failed: %d", "mpack_writer_destroy", status);
        return;
    }

    // copy to output buffer
    out.resize(size);
    out.assign(reinterpret_cast<std::byte *>(data), reinterpret_cast<std::byte *>(data + size));
    free(data);
}


