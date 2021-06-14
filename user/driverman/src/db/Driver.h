#ifndef DB_DRIVER_H
#define DB_DRIVER_H

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

class Device;
class DeviceMatch;

class DriverInstance;

/**
 * Represents a single device driver, capable of matching against any number of devices.
 */
class Driver: public std::enable_shared_from_this<Driver> {
    public:
        Driver(const std::string &path);
        virtual ~Driver();

        /// Adds a new match object to the driver. We take ownership of the pointer.
        void addMatch(DeviceMatch *);

        /// Determine whether this driver matches against the device, and if so, its priority.
        virtual bool test(const std::shared_ptr<Device> &dev, int &outPriority);
        /// Start the driver for the given device.
        virtual void start(const std::shared_ptr<Device> &dev);

        /// Return the path to the driver binary.
        const std::string &getPath() const {
            return this->path;
        }

    private:
        std::shared_ptr<DriverInstance> makeInstance(const std::shared_ptr<Device> &);

    protected:
        /// Driver binary path
        std::string path;

        // A list of match objects that we can use to match against a device.
        std::vector<DeviceMatch *> matches;
        /// whethen set, all match descriptors must match; if clear, only one must
        bool mustMatchAll{false};

        /// when set, the driver does not share tasks
        bool alwaysLaunchNew{false};
        /// all driver instances
        std::vector<std::shared_ptr<DriverInstance>> instances;
};

#endif
