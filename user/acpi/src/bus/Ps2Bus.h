#ifndef BUS_PS2BUS_H
#define BUS_PS2BUS_H

#include "Bus.h"

#include <cstddef>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <acpi.h>

namespace acpi {
/**
 * Represents a PS/2 keyboard and mouse controller.
 */
class Ps2Bus: public Bus {
    private:
        static const std::string kBusName;
        static const std::string kDriverName;

        /// resources we can assign a PS/2 controller
        using Resource = std::variant<ACPI_RESOURCE_IRQ, ACPI_RESOURCE_IO>;

    public:
        /// Probes the ACPI tables to see if a PS/2 controller exists.
        static std::shared_ptr<Ps2Bus> probe();

        /// Creates a new PS/2 bus.
        Ps2Bus(Bus * _Nullable parent, ACPI_HANDLE _Nonnull kbd);

        void loadDriver(const uintptr_t id) override;

        /// Name of the bus
        const std::string &getName() const override {
            return kBusName;
        }

    private:
        /// Extracts consumed resources from the ACPI object.
        void extractResources(ACPI_HANDLE _Nonnull, std::vector<Resource> &);
        /// Serializes the driver aux data (resource assignments)
        void serializeAuxData(std::vector<std::byte> &out);

    private:
        /**
         * Resources requested by the keyboard controller. We assume this is where the IO ports are
         * defined (for command and data) as well as the the interrupt for the keyboard port.
         */
        std::vector<Resource> kbdResources;
        /**
         * If the controller supports mice, additional resources to do so are defined here. In
         * normal controllers, this is just an additional interrupts
         */
        std::vector<Resource> mouseResources;
};
}

#endif
