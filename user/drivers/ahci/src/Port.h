#ifndef AHCIDRV_PORT_H
#define AHCIDRV_PORT_H

#include <cstddef>
#include <cstdint>

class Controller;

/**
 * Handles transactions for a single port on an AHCI controller.
 *
 * Each port has its own private allocation for the command list and received FIS structures, while
 * command lists are drawn from a global pool shared between all ports on a controller.
 */
class Port {
    public:
        Port(Controller * _Nonnull controller, const uint8_t port);
        virtual ~Port();

    private:
        static uintptr_t kPrivateMappingRange[2];

        /// Port number on the controller
        uint8_t port{0};
        /// VM region handle containing the command list and received FIS structures
        uintptr_t privRegionVmHandle{0};

        /// AHCI controller on which this port is
        Controller * _Nonnull parent;
};

#endif
