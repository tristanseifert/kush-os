#include "PciBus.h"

#include "acpi.h"
#include "log.h"

using namespace acpi;

std::string const PciBus::kBusName = "PCI";

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

        Trace("Device %2u: INTA %2d INTB %2d INTC %2d INTD %2d", device, a, b, c, d);
    }
}
