#ifndef AHCIDRV_DEVICE_H
#define AHCIDRV_DEVICE_H

#include <cstddef>
#include <cstdint>
#include <memory>

class Port;

/**
 * Base interface for a device attached to a port on an AHCI controller; this may be a typical
 * block device like a hard drive or solid state disk; or it can be a packet based device such as
 * an optical or a tape drive.
 *
 * The specific behavior (and the behavior to interface with the rest of the world via RPC to
 * configure the device, and shared memory to transfer data) is implemented by the concrete
 * subclasses.
 */
class Device {
    public:
        /// Creates a new device attached to the given AHCI port.
        Device(const std::shared_ptr<Port> _port) : port(_port) {}
        virtual ~Device() = default;

    protected:
        /**
         * Port to which this device is connected. We keep a weak reference so that the port is
         * free to go away (if the controller gets deallocated, for example) and we can fail any
         * subsequent IO operations.
         */
        std::weak_ptr<Port> port;
};

#endif
