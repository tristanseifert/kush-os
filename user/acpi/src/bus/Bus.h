#ifndef BUS_BUS_H
#define BUS_BUS_H

#include <cstdint>
#include <string>

namespace acpi {
/**
 * Base class for all discovered busses
 */
class Bus {
    public:
        Bus(Bus * _Nullable _parent, const std::string &path) : parent(_parent),
            acpiPath(path) {}

        virtual ~Bus() = default;

        /// Sends the driver server a discover message about this bus.
        virtual void loadDriver(const uintptr_t id) = 0;

        /// Returns the bus name
        virtual const std::string &getName() const = 0;
        /// Returns the ACPI path at which the bus was found.
        virtual const std::string &getAcpiPath() const {
            return this->acpiPath;
        }

    protected:
        /**
         * Bus to which this bus is connected, if any. This may not necessarily be a bus of the
         * same type: for example, a PC may have an ISA bus behind a PCI <-> ISA bridge.
         */
        Bus * _Nullable parent = nullptr;

        /// ACPI path
        std::string acpiPath;
};
}

#endif
