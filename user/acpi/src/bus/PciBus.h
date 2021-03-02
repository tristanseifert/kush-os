#ifndef BUS_PCIBUS_H
#define BUS_PCIBUS_H

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "acpi.h"
#include "Bus.h"
#include "resource/Irq.h"

namespace acpi {
/**
 * PCI bus: these are discovered during the scanning of the ACPI tables for root ports. Each bus
 * has associated with it its interrupt mappings.
 */
class PciBus: public Bus {
    private:
        static const std::string kBusName;

    public:
        /**
         * Initializes a new PCI bus instance.
         *
         * The given bus number is assigned as the base for this bus.
         */
        PciBus(Bus * _Nullable parent, const std::string &acpiPath, const uint8_t _bus) :
            Bus(parent, acpiPath), bus(_bus) {
            // nothing... yet
        }

        /// Given a PCI bridge object in the ACPI tables, extract its interrupt routings
        void getIrqRoutes(_Nonnull ACPI_HANDLE);

        /// Whether we have an IRQ map
        const bool hasIrqMap() const {
            return !this->irqMap.empty();
        }
        /// Name of the bus
        const std::string &getName() const override {
            return kBusName;
        }

    private:
        /**
         * Describes the mapping of a device's PCI interrupts (that is, the #INTA-D pins) to the
         * host system's interrupt numbers.
         */
        struct DeviceIrqInfo {
            /// IRQ associated with INTA
            std::optional<resource::Irq> inta;
            /// IRQ associated with INTB
            std::optional<resource::Irq> intb;
            /// IRQ associated with INTC
            std::optional<resource::Irq> intc;
            /// IRQ associated with INTD
            std::optional<resource::Irq> intd;
        };

    private:
        /// whether interrupt mappings are logged
        static bool gLogInterrupts;

    private:
        /// Bus number
        uint8_t bus = 0;

        /// Interrupt mappings from device IRQ pins to host IRQs
        std::unordered_map<uint8_t, DeviceIrqInfo> irqMap;
};
}

#endif
