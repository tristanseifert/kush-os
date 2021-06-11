#include "Ps2Bus.h"
#include "AcpiUtils.h"
#include "resource/Serialize.h"

#include "log.h"

#include <type_traits>

#include <driver/DrivermanClient.h>
#include <mpack/mpack.h>

#include <acpi.h>

using namespace acpi;

std::string const Ps2Bus::kBusName = "PS2";
std::string const Ps2Bus::kDriverName = "AcpiPs2Controller";

template<class> inline constexpr bool always_false_v = false;

/**
 * Searches the ACPI tables for a PS/2 keyboard and mouse controller. We'll first try to detect the
 * keyboard controller, then the mouse part.
 *
 * We assume the keyboard controller will be found under any of the "PNP0303" PNP IDs, and that it
 * will hold the IO port reservations.
 *
 * We also assume the mouse port is listed as the "" PNP ID, and contains the interrupt routing
 * information for the mouse port.
 */
std::shared_ptr<Ps2Bus> Ps2Bus::probe() {
    ACPI_STATUS status;
    void *retval = nullptr;

    // search for the keyboard controller
    status = AcpiGetDevices("PNP0303",
            [](ACPI_HANDLE obj, UINT32 level, void *ctx, void **ret) -> ACPI_STATUS {
        *ret = obj;
        return 420;
    }, nullptr, &retval);
    if (ACPI_FAILURE(status) && status != 420) {
        Abort("%s failed: %s", "AcpiGetDevices", AcpiFormatException(status));
    }

    if(!retval) {
        // no PS/2 controller
        Trace("No PS/2 keyboard controller found in ACPI");
        return nullptr;
    }

    // set up the PS/2 bus with the keyboard controller
    ACPI_HANDLE kbd = reinterpret_cast<ACPI_HANDLE>(retval);
    auto bus = std::make_shared<Ps2Bus>(nullptr, kbd);

    // try to find a mouse controller
    retval = nullptr;

    status = AcpiGetDevices("PNP0F13",
            [](ACPI_HANDLE obj, UINT32 level, void *ctx, void **ret) -> ACPI_STATUS {
        *ret = obj;
        return 420;
    }, nullptr, &retval);
    if (ACPI_FAILURE(status) && status != 420) {
        Warn("failed to find PS/2 mouse controller: %s", AcpiFormatException(status));
        return bus;
    } 

    if(!retval) {
        // no mouse controller found
        Trace("No mouse detected for PS/2");
        return bus;
    }

    // get mouse resources
    ACPI_HANDLE mouse = reinterpret_cast<ACPI_HANDLE>(retval);
    bus->extractResources(mouse, bus->mouseResources);

    return bus;
}

/**
 * Initializes a new PS2 bus controller, based on the information stored in a keyboard controller
 * object in the ACPI table.
 */
Ps2Bus::Ps2Bus(Bus *parent, ACPI_HANDLE kbd) : Bus(parent, AcpiUtils::getName(kbd)){
    // extract resource assignments
    this->extractResources(kbd, this->kbdResources);
}

/**
 * Extracts resource information from the given ACPI object.
 */
void Ps2Bus::extractResources(ACPI_HANDLE object, std::vector<Resource> &outRsrc) {
    ACPI_STATUS status;
    ACPI_BUFFER buf;

    // extract the resource assignments
    buf.Length = ACPI_ALLOCATE_BUFFER;
    buf.Pointer = nullptr;

    status = AcpiGetCurrentResources(object, &buf);
    if(status != AE_OK) {
        Abort("%s failed: %s", "AcpiGetCurrentResources", AcpiFormatException(status));
    }

    // iterate over all resources
    auto rsrc = reinterpret_cast<const ACPI_RESOURCE *>(buf.Pointer);

    while(true) {
        // bail if end tag
        if(rsrc->Type == ACPI_RESOURCE_TYPE_END_TAG) {
            break;
        }

        // insert the object
        switch(rsrc->Type) {
            case ACPI_RESOURCE_TYPE_IRQ:
                outRsrc.push_back(rsrc->Data.Irq);
                break;
            case ACPI_RESOURCE_TYPE_IO:
                outRsrc.push_back(rsrc->Data.Io);
                break;

            default:
                Warn("unsupported PS/2 resource type: %u", rsrc->Type);
        }

        // go to next
        rsrc = ACPI_NEXT_RESOURCE(rsrc);
    }

    // clean up
    ACPI_FREE(buf.Pointer);
}



/**
 * Load the PS/2 bus driver.
 */
void Ps2Bus::loadDriver(const uintptr_t id) {
    std::vector<std::byte> aux;
    this->serializeAuxData(aux);

    this->drivermanPath = libdriver::RpcClient::the()->AddDevice(kAcpiBusRoot, kDriverName, aux);
    Trace("PS/2 bus registered at %s", this->drivermanPath.c_str());
}

/**
 * Serializes our hardware resources information.
 */
void Ps2Bus::serializeAuxData(std::vector<std::byte> &out) {
    char *data;
    size_t size;

    // set up writer
    mpack_writer_t writer;
    mpack_writer_init_growable(&writer, &data, &size);

    mpack_start_map(&writer, 2);

    // write the keyboard resources
    mpack_write_cstr(&writer, "kbd");
    if(this->kbdResources.empty()) {
        mpack_write_nil(&writer);
    } else {
        mpack_start_array(&writer, this->kbdResources.size());
        for(const auto &resource : this->kbdResources) {
            std::visit([&](auto&& arg) -> void {
                serialize(&writer, arg);
            }, resource);
        }
        mpack_finish_array(&writer);
    }

    // write the mouse resources
    mpack_write_cstr(&writer, "mouse");
    if(this->mouseResources.empty()) {
        mpack_write_nil(&writer);
    } else {
        mpack_start_array(&writer, this->mouseResources.size());
        for(const auto &resource : this->mouseResources) {
            std::visit([&](auto&& arg) -> void {
                serialize(&writer, arg);
            }, resource);
        }
        mpack_finish_array(&writer);
    }

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


