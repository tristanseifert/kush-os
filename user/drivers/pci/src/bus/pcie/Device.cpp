#include "Device.h"
#include "PciExpressBus.h"

#include "Log.h"
#include "bus/PciConfig.h"

#include <cstdlib>
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
    const uint16_t classes = c->read(this->address, 0xA, PciConfig::Width::Word);
    const uint8_t subclass = (classes & 0xFF), classCode = (classes >> 8);

    // build the string to advertise under
    char nameBuf[80];
    snprintf(nameBuf, sizeof(nameBuf), "PciExpress%04x.%04x@%x.%x.%x,GenericPciExpressDevice", vid,
            pid, _address.bus, _address.device, _address.function);

    // advertise the device and set its information
    auto rpc = libdriver::RpcClient::the();

    std::vector<std::byte> aux;
    this->serializeAuxData(aux, vid, pid, classCode, subclass);

    this->path = rpc->AddDevice(_bus->getForestPath(), nameBuf);
    if(kLogPaths) Trace("PCI device %s registered as %s", nameBuf, this->path.c_str());

    rpc->SetDeviceProperty(this->path, kPciAddressPropertyName, aux);

    rpc->StartDevice(this->path);
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
void Device::serializeAuxData(std::vector<std::byte> &out, const uint16_t vid, const uint16_t pid,
        const uint8_t classId, const uint8_t subclassId) {
    char *data;
    size_t size;

    const auto &a = this->address;

    // set up writer
    mpack_writer_t writer;
    mpack_writer_init_growable(&writer, &data, &size);

    mpack_start_map(&writer, 8);
    mpack_write_cstr(&writer, "segment");
    mpack_write_u16(&writer, a.segment);
    mpack_write_cstr(&writer, "bus");
    mpack_write_u8(&writer, a.bus);
    mpack_write_cstr(&writer, "device");
    mpack_write_u8(&writer, a.device);
    mpack_write_cstr(&writer, "function");
    mpack_write_u8(&writer, a.function);

    mpack_write_cstr(&writer, "vid");
    mpack_write_u16(&writer, vid);
    mpack_write_cstr(&writer, "pid");
    mpack_write_u16(&writer, pid);
    mpack_write_cstr(&writer, "class");
    mpack_write_u8(&writer, classId);
    mpack_write_cstr(&writer, "subclass");
    mpack_write_u8(&writer, subclassId);

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


