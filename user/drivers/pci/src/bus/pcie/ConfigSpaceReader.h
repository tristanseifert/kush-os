#ifndef PCISRV_BUS_PCIE_CONFIGSPACEREADER_H
#define PCISRV_BUS_PCIE_CONFIGSPACEREADER_H

#include <cstddef>
#include <memory>

#include <libpci/UserClientTypes.h>
#include "bus/PciConfig.h"

class PciExpressBus;

namespace pcie {
/**
 * Provides an interface to reading PCIe configuration space of a particular bus. This is done via
 * the mapped ECAM region.
 */
class ConfigSpaceReader: public PciConfig {
    using DeviceAddress = libpci::BusAddress;

    public:
        ConfigSpaceReader(PciExpressBus *);

        uint64_t read(const DeviceAddress &device, const size_t reg, const Width width) override;
        void write(const DeviceAddress &device, const size_t reg, const Width width,
                const uint64_t value) override;

    private:
        static void EnsureOnBus(PciExpressBus *, const DeviceAddress &);
        static void *GetEcamAddr(PciExpressBus *, const DeviceAddress &, const size_t, const Width);

    private:
        // Bus that we perform the config space accesses on.
        PciExpressBus* bus;
};
}

#endif
