#ifndef PCIESRV_BUSREGISTRY_H
#define PCIESRV_BUSREGISTRY_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include <libpci/UserClientTypes.h>

class PciExpressBus;

/**
 * Holds references to all PCI and PCI Express busses on the system.
 *
 * It allows quickly looking up the bus that is responsible for handling a device by its address.
 */
class BusRegistry {
    using DeviceAddress = libpci::BusAddress;

    public:
        /// Initialize the shared instance of the bus registry
        static void init();
        /// Return the shared instance of the bus registry.
        static auto *the() {
            return gShared;
        }

        /// Registers a PCI Express bus.
        void add(const std::shared_ptr<PciExpressBus> &bus);

        /// Returns the PCI bus responsible for the given device.
        std::shared_ptr<PciExpressBus> get(const DeviceAddress &addr) const;

        /// Scans for devices on all busses.
        size_t scanAll();

    private:
        /**
         * Represents a single bus segment, which may contain one or more busses. Each bus object
         * can actually handle more than one physical bus, in the case of root bridges or busses
         * that contain bridges.
         *
         * Busses are stored in a list as usually segments only have very few busses and the
         * overhead of maintaining a separate map is too much effort.
         */
        struct Segment {
            /// A [min, max] range of busses
            using BusRange = std::pair<uint8_t, uint8_t>;
            /// List of busses on this segment
            std::vector<std::shared_ptr<PciExpressBus>> busses;

            /// Find the bus responsible for the given device.
            std::shared_ptr<PciExpressBus> getBusFor(const DeviceAddress &addr) const;
        };

    private:
        BusRegistry() = default;
        ~BusRegistry() = default;

    private:
        static BusRegistry *gShared;

        /// All PCIe busses in the system
        std::vector<std::shared_ptr<PciExpressBus>> pcie;

        /// Map of all bus segments in the system
        std::unordered_map<uint16_t, Segment> segments;
};

#endif
