#ifndef BUS_PCIEXPRESSBUS_H
#define BUS_PCIEXPRESSBUS_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

#include "acpi.h"
#include "Bus.h"

struct mpack_writer_t;

namespace acpi {
/**
 * PCI Express bus, discovered via the MCFG table. The memory mapped config space is made available
 * which is sufficient to discover devices on the bus and interact with all components on the bus.
 *
 * Technically, this corresponds to a root bridge, one per MCFG entry.
 */
class PciExpressBus: public Bus {
    private:
        static const std::string kBusName;
        static const std::string kDriverName;

        /// key to the device specific aux data property
        constexpr static const std::string_view kAuxDataKey{"pcie.resources"};

        /// produce logging when the bus is registered
        constexpr static const bool gLogRegister{false};

    public:
        /**
         * Initializes a PCI Express root bridge segment from an ACPI MCFG entry.
         */
        PciExpressBus(Bus * _Nullable parent, const std::string &acpiPath,
                const ACPI_MCFG_ALLOCATION &m): Bus(parent, acpiPath), configAperture(m.Address),
                segment(m.PciSegment), busses({m.StartBusNumber, m.EndBusNumber}) {}

        /// Name of the bus
        const std::string &getName() const override {
            return kBusName;
        }

        /// Launches the PCI driver.
        void loadDriver(const uintptr_t) override;

    private:
        /// Serializes the driver aux data (resource assignments)
        void serializeAuxData(std::vector<std::byte> &out);

    private:
        /// Physical base address of the configuration aperture
        uint64_t configAperture{0};

        /// PCIe segment this bridge represents
        uint16_t segment{0};
        /// Range of busses on that segment this bridge controls
        std::pair<uint8_t, uint8_t> busses{0, 255};
};
}

#endif
