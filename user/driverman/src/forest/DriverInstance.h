#ifndef FOREST_DRIVERINSTANCE_H
#define FOREST_DRIVERINSTANCE_H

#include <cstddef>
#include <memory>

class Driver;

/**
 * Driver instances encapsulate the connection and task information for the driver for a device.
 * They correspond to a driver in the driver database, and support sharing one instance object
 * between multiple distinct devices.
 *
 * It's assumed that regardless of the number of devices, each driver will only be launched once;
 * any subsequent discovered devices that this driver matches on will simply be sent to the driver
 * port (passed to the driver as an argument when created) and the driver can then act on this to
 * start or stop devices.
 *
 * Some drivers only need to be launched once -- think device firmware, power management, etc. --
 * these will simply not have a driver port associated with them.
 */
class DriverInstance: public std::enable_shared_from_this<DriverInstance> {
    using InstPtr = std::shared_ptr<DriverInstance>;

    public:
        // Create a driver instance with an already created task.
        DriverInstance(const std::shared_ptr<Driver> &driver, const uintptr_t task);


        virtual ~DriverInstance() = default;

    private:
        /// original driver object (weak so we don't create a retain cycle)
        std::weak_ptr<Driver> driver;
        /// Task handle of the driver task, or 0 if built in
        uintptr_t taskHandle{0};
};

#endif
