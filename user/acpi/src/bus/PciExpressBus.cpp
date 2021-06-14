#include "PciExpressBus.h"

#include "acpi.h"
#include "log.h"

#include <driver/DrivermanClient.h>
#include <mpack/mpack.h>

using namespace acpi;

std::string const PciExpressBus::kBusName = "PCI Express";
std::string const PciExpressBus::kDriverName = "AcpiPciExpressRootBridge,GenericPciExpressRootBridge";

/**
 * Loads the PCI Express driver.
 */
void PciExpressBus::loadDriver(const uintptr_t) {
    std::vector<std::byte> aux;
    this->serializeAuxData(aux);

    auto rpc = libdriver::RpcClient::the();

    // register driver
    this->drivermanPath = rpc->AddDevice(kAcpiBusRoot, kDriverName);
    Trace("PCIe bus registered at %s", this->drivermanPath.c_str());

    // set configuration
    rpc->SetDeviceProperty(this->drivermanPath, kAuxDataKey, aux);
}


/**
 * Serializes the config aperture and bus range information.
 */
void PciExpressBus::serializeAuxData(std::vector<std::byte> &out) {
    char *data;
    size_t size;

    // set up writer
    mpack_writer_t writer;
    mpack_writer_init_growable(&writer, &data, &size);

    mpack_start_map(&writer, 4);
    mpack_write_cstr(&writer, "busMin");
    mpack_write_u8(&writer, this->busses.first);
    mpack_write_cstr(&writer, "busMax");
    mpack_write_u8(&writer, this->busses.second);
    mpack_write_cstr(&writer, "segment");
    mpack_write_u16(&writer, this->segment);
    mpack_write_cstr(&writer, "ecamAddr");
    mpack_write_u64(&writer, this->configAperture);

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
