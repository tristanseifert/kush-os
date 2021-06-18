#ifndef AHCIDRV_CONTROLLER_H
#define AHCIDRV_CONTROLLER_H

#include <array>
#include <cstdint>
#include <cstddef>
#include <memory>

#include <libpci/UserClient.h>

struct AhciHbaRegisters;

class Port;

/**
 * Encapsulates the main behavior for an AHCI controller.
 *
 * This class is primarily responsible for overall controller initialization, interrupt management,
 * and the setup of the individual ports. Each port functions almost independently with its own
 * memory resources.
 */
class Controller {
    friend class Port;

    /// Maximum number of ports an AHCI controller supports
    constexpr static const size_t kMaxPorts{32};

    public:
        /// Create an AHCI controller attached to the given PCI device.
        Controller(const std::shared_ptr<libpci::Device> &);
        virtual ~Controller();

    private:
        void performBiosHandoff();
        void reset();

    private:
        static const uintptr_t kAbarMappingRange[2];

        /// PCI device behind which the controller is operated
        std::shared_ptr<libpci::Device> dev;

        /// VM handle of the ABAR region for the device
        uintptr_t abarVmHandle{0};
        /// Base address of the AHCI HBA registers
        volatile AhciHbaRegisters *abar{nullptr};

        /// Bitmap of which ports are valid and implemented on the HBA
        uint32_t validPorts{0};

        /// Whether the HBA supports 64-bit addressing
        bool supports64Bit{false};
        /// Whether the HBA supports SATA native command queuing
        bool supportsNCQ{false};
        /// Whether the HBA supports the SATA Notification register
        bool supportsSataNotifications{false};
        /// Whether staggered spinup is supported
        bool supportsStaggeredSpinup{false};

        /// Maximum supported SATA generation (1 = 1.5Gbps, 2 = 3Gbps, 3 = 6Gbps)
        size_t sataGen{0};

        /// Each implemented port has an allocated port object
        std::array<std::shared_ptr<Port>, kMaxPorts> ports;
};

#endif
