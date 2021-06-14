#ifndef DB_DEVICEMATCH_H
#define DB_DEVICEMATCH_H

#include <memory>
#include <string>

class Device;

/**
 * Defines the basic interface of a device match structure. Each match implements a different
 * test method that checks if the provided device can be supported by the driver.
 */
class DeviceMatch {
    public:
        virtual ~DeviceMatch() = default;

        /**
         * Tests if the driver can support the given device. If so, its priority relative to other
         * drivers is output as well. This allows multiple drivers: imagine a generic driver that
         * allows basic feature support for a wide range of hardware, with more specific drivers
         * with higher priorities for more specific hardware.
         */
        virtual bool supportsDevice(const std::shared_ptr<Device> &dev, int &outPriority) = 0;
};

/**
 * Matches a device based on the name of the driver.
 */
class DeviceNameMatch: public DeviceMatch {
    public:
        /**
         * Creates a match object that will match if any of the device's object names are found.
         */
        DeviceNameMatch(const std::string &_name, int _priority) : name(_name), priority(_priority) {}

        bool supportsDevice(const std::shared_ptr<Device> &dev, int &outPriority) override;

    private:
        /// Name to match against anywhere in the device name list
        std::string name;
        /// Constant priority value to output
        int priority{0};
};

#endif
