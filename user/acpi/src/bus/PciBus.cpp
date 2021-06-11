#include "PciBus.h"

#include "acpi.h"
#include "log.h"

#include <driver/DrivermanClient.h>
#include <mpack/mpack.h>

using namespace acpi;

std::string const PciBus::kBusName = "PCI";
std::string const PciBus::kDriverName = "AcpiPciRootBridge";

bool PciBus::gLogInterrupts = false;

/**
 * Extracts the interrupt routing from a given ACPI object.
 *
 * @param object Handle to the PCI object (such as "\_SB.PCI0") in the ACPI namespace.
 */
void PciBus::getIrqRoutes(ACPI_HANDLE object) {
    ACPI_STATUS status;

    // set up the buffer
    ACPI_BUFFER buf;
    buf.Length = ACPI_ALLOCATE_BUFFER;
    buf.Pointer = nullptr;

    // read tables
    status = AcpiGetIrqRoutingTable(object, &buf);
    if(status != AE_OK) {
        Abort("AcpiGetIrqRoutingTable failed: %s", AcpiFormatException(status));
    }

    // iterate over the interrupts
    std::byte *scan = reinterpret_cast<std::byte *>(buf.Pointer);

    while(true) {
        // get table and exit if we've reached the end
        auto table = reinterpret_cast<const ACPI_PCI_ROUTING_TABLE *>(scan);
        if(!table->Length) {
            break;
        }

        uint8_t slot = (table->Address >> 16);

        // static interrupt assignment
        if(!table->Source[0]) {
            // TODO: implement static IRQs
            Trace("gsi %u pin %u (slot %u)", table->SourceIndex, table->Pin, slot);
        }
        // the assignment is dynamic
        else {
            // get PCI link
            ACPI_HANDLE linkObject;
            status = AcpiGetHandle(object, table->Source, &linkObject);
            if(status != AE_OK) {
                Abort("failed to get source '%s': %s", table->Source, AcpiFormatException(status));
            }

            // get associated IRQ
            ACPI_BUFFER resbuf;
            resbuf.Length = ACPI_ALLOCATE_BUFFER;
            resbuf.Pointer = NULL;

            status = AcpiGetCurrentResources(linkObject, &resbuf);
            if(status != AE_OK) {
                Abort("AcpiGetCurrentResources failed for '%s': %s", table->Source, AcpiFormatException(status));
            }

            auto rscan = reinterpret_cast<std::byte *>(resbuf.Pointer);
            resource::Irq devIrq;
            while(1) {
                // bail if at end
                auto res = reinterpret_cast<ACPI_RESOURCE *>(rscan);
                if(res->Type == ACPI_RESOURCE_TYPE_END_TAG) {
                    break;
                }
                // handle interrupt resource types
                else if(res->Type == ACPI_RESOURCE_TYPE_IRQ) {
                    devIrq = resource::Irq(res->Data.Irq);
                    break;
                }
                else if(res->Type == ACPI_RESOURCE_TYPE_EXTENDED_IRQ) {
                    devIrq = resource::Irq(res->Data.ExtendedIrq);
                    break;
                }

                // check next resource
                rscan += res->Length;
            }

            // release resource buffer
            free(resbuf.Pointer);

            // record the interrupt we got
            if(devIrq.flags == resource::IrqMode::Invalid) {
                Abort("failed to derive IRQ for device %d from '%s'", (int)slot, table->Source);
            }

            // modify an IRQ map entry given the current state
            auto yeeter = [&, devIrq](DeviceIrqInfo &map) {
                switch(table->Pin) {
                    case 0:
                        map.inta = devIrq;
                        break;
                    case 1:
                        map.intb = devIrq;
                        break;
                    case 2:
                        map.intc = devIrq;
                        break;
                    case 3:
                        map.intd = devIrq;
                        break;
                }
            };

            // have we an existing object?
            if(this->irqMap.contains(slot)) {
                auto &map = this->irqMap.at(slot);
                yeeter(map);
            } else {
                DeviceIrqInfo map;
                yeeter(map);

                this->irqMap.emplace(slot, std::move(map));
            }
        }

        // go to next
        scan += table->Length;
    }

    // clean up
    free(buf.Pointer);

    // XXX: print
    for(const auto &[device, map] : this->irqMap) {
        int a = -1, b = -1, c = -1, d = -1;
        if(map.inta) {
            a = (*map.inta).irq;
        } if(map.intb) {
            b = (*map.intb).irq;
        } if(map.intc) {
            c = (*map.intc).irq;
        } if(map.intd) {
            d = (*map.intd).irq;
        }

        if(gLogInterrupts) {
            Trace("Device %2u: INTA %2d INTB %2d INTC %2d INTD %2d", device, a, b, c, d);
        }
    }
}

/**
 * Loads a driver for this PCI bus.
 */
void PciBus::loadDriver(const uintptr_t) {
    std::vector<std::byte> aux;
    this->serializeAuxData(aux);

    this->drivermanPath = libdriver::RpcClient::the()->AddDevice(kAcpiBusRoot, kDriverName, aux);
    Trace("PCI bus registered at %s", this->drivermanPath.c_str());
}

/**
 * Serializes the interrupt map to an msgpack object. It's basically identical to the
 * representation used in memory. Additionally, bus address is added.
 */
void PciBus::serializeAuxData(std::vector<std::byte> &out) {
    char *data;
    size_t size;

    // set up writer
    mpack_writer_t writer;
    mpack_writer_init_growable(&writer, &data, &size);

    mpack_start_map(&writer, 4);
    mpack_write_cstr(&writer, "bus");
    mpack_write_u8(&writer, this->bus);
    mpack_write_cstr(&writer, "segment");
    mpack_write_u8(&writer, this->segment);
    mpack_write_cstr(&writer, "address");
    mpack_write_u32(&writer, this->address);

    // serialize
    mpack_write_cstr(&writer, "irqs");
    if(!this->irqMap.empty()) {
        mpack_start_map(&writer, this->irqMap.size());

        for(const auto &[device, info] : this->irqMap) {
            mpack_write_u8(&writer, device);
            info.serialize(&writer);
        }

        mpack_finish_map(&writer);
    } else {
        mpack_write_nil(&writer);
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

/**
 * Serializes an interrupt info object.
 */
void PciBus::DeviceIrqInfo::serialize(mpack_writer_t *writer) const {
    mpack_start_map(writer, 4);

    // INTA
    //mpack_write_cstr(writer, "a");
    mpack_write_u8(writer, 0);
    if(this->inta) {
        this->inta->serialize(writer);
    } else {
        mpack_write_nil(writer);
    }

    // INTB
    //mpack_write_cstr(writer, "b");
    mpack_write_u8(writer, 1);
    if(this->intb) {
        this->intb->serialize(writer);
    } else {
        mpack_write_nil(writer);
    }

    // INTC
    //mpack_write_cstr(writer, "c");
    mpack_write_u8(writer, 2);
    if(this->intc) {
        this->intc->serialize(writer);
    } else {
        mpack_write_nil(writer);
    }

    // INTD
    //mpack_write_cstr(writer, "d");
    mpack_write_u8(writer, 3);
    if(this->intd) {
        this->intd->serialize(writer);
    } else {
        mpack_write_nil(writer);
    }

    // clean up
    mpack_finish_map(writer);
}
