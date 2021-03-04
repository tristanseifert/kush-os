#ifndef BUS_ROOTBRIDGE_H
#define BUS_ROOTBRIDGE_H

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <span>

struct mpack_node_t;

/**
 * Processes events for a PCI root bridge, including enumeration of devices located under it.
 */
class RootBridge {
    public:
        /// Create a new root bridge with the given auxiliary data.
        RootBridge(const std::span<std::byte> &aux);

        /// Scans all devices on the bus.
        void scan();

    private:
        /**
         * Info on a single IRQ
         */
        struct Irq {
            /// System interrupt number
            uintptr_t num;

            Irq() = default;
            Irq(mpack_node_t &);
        };

        /**
         * Interrupt info for a particular PCI device
         */
        struct IrqInfo {
            std::optional<Irq> inta;
            std::optional<Irq> intb;
            std::optional<Irq> intc;
            std::optional<Irq> intd;
        };

    private:
        /// when set, IRQ map entries are logged.
        static bool gLogIrqMap;

    private:
        /// address of the bus behind the bridge
        uint8_t bus;
        /// bus segment (if any)
        uint8_t segment = 0;
        /// device address (high = device, low word = function) of the bus bridge (if any)
        uint32_t address = 0;

        /// Interrupt mappings for each device on the bus, if available.
        std::unordered_map<uint8_t, IrqInfo> irqMap;
};

#endif
