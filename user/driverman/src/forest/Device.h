#ifndef FOREST_DEVICE_H
#define FOREST_DEVICE_H

#include <cstddef>
#include <memory>
#include <optional>
#include <unordered_map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Forest.h"

class DriverInstance;

/**
 * Each node in the forest has an associated device. These are lightweight objects that hold some
 * key/value properties, and an identifier used for driver matching. Drivers may store specific
 * data as key/value pairs.
 */
class Device: public std::enable_shared_from_this<Device> {
    using ByteSpan = std::span<std::byte>;
    using ByteVec = std::vector<std::byte>;

    /// separator character for driver names
    constexpr static const char kDriverNameSeparator{','};

    public:
        /// Create a new device with the given match string.
        Device(const std::string &drivers) {
            SplitDriverName(drivers, this->driverNames);
        }
        /// Create a new device with the given match string.
        Device(const std::string_view &drivers) {
            SplitDriverName(drivers, this->driverNames);
        }
        /// Cleans up device resources.
        virtual ~Device() = default;

        /// Attempts to find and start a driver for the device.
        virtual bool findAndLoadDriver();

        /// Gets the device's path in the forest if it is contained within
        std::optional<std::string> getPath();

        /// Get the primary driver name, used as part of the device's path.
        constexpr const std::string &getPrimaryName() const {
            return this->driverNames[0];
        }
        /// Gets all driver names.
        constexpr const std::vector<std::string> &getDriverNames() const {
            return this->driverNames;
        }

        /// Sets a property, overwriting it if it already exists.
        void setProperty(const std::string &key, const ByteSpan &_data) {
            std::vector<std::byte> data(_data.begin(), _data.end());
            this->setProperty(key, data);
        }
        /// Sets a property, overwriting it if it already exists.
        void setProperty(const std::string &key, const ByteVec &data) {
            if(!this->properties.contains(key)) [[likely]] {
                this->properties.emplace(key, data);
            } else {
                this->properties[key] = data;
            }
        }
        /// Deletes the given property, if it exists.
        void removeProperty(const std::string &key) {
            this->properties.erase(key);
        }
        /// Tests if the given property exists.
        bool hasProperty(const std::string &key) const {
            return this->properties.contains(key);
        }
        /// Get the value of a property.
        ByteVec getProperty(const std::string &key) const {
            if(!this->properties.contains(key)) {
                return {};
            }
            return this->properties.at(key);
        }

        /// Sets the driver associated with the device.
        void setDriver(const std::shared_ptr<DriverInstance> &newDriver) {
            this->driver = newDriver;
        }
        /// Tests if we have an assigned driver.
        constexpr bool hasDriver() const {
            return !!this->driver;
        }
        /// Returns the driver associated with the device.
        auto getDriver() const {
            return this->driver;
        }

        /// The device is about to be removed from the given forest node.
        virtual void willDeforest(const std::shared_ptr<Forest::Leaf> &leaf);
        /// The device has been added to a forest node.
        virtual void didReforest(const std::shared_ptr<Forest::Leaf> &leaf);

    protected:
        /// Splits the given driver match string into an ordered list.
        static void SplitDriverName(const std::string_view &, std::vector<std::string> &);

    protected:
        /// If the device is in the forest, the leaf it is stored under
        std::weak_ptr<Forest::Leaf> leaf;

        /// Device match strings, in descending order of precedence.
        std::vector<std::string> driverNames;
        /// The current driver instance operating the device
        std::shared_ptr<DriverInstance> driver{nullptr};

        /// Key/value properties associated with the device
        std::unordered_map<std::string, ByteVec> properties;
};

#endif
