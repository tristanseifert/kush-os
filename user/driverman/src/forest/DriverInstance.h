#ifndef FOREST_DRIVERINSTANCE_H
#define FOREST_DRIVERINSTANCE_H

#include <cstddef>
#include <memory>

/**
 * Driver instances encapsulate the connection and task information for the driver for a device.
 * They correspond to a driver in the driver database, and support sharing one instance object
 * between multiple distinct devices.
 */
class DriverInstance: public std::enable_shared_from_this<DriverInstance> {
    public:
        virtual ~DriverInstance() = default;

    private:
        /// Task handle of the driver task, or 0 if built in
        uintptr_t taskHandle{0};
};

#endif
