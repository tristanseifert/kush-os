#ifndef DB_DRIVER_H
#define DB_DRIVER_H

#include <driver/Discovery.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

/**
 * Represents a single device driver, capable of matching against any number of devices.
 */
class Driver {
    public:
        Driver(const std::string &path);
        virtual ~Driver();

        /// Adds a new match object to the driver. We take ownership of the pointer.
        void addMatch(libdriver::DeviceMatch *);
        /// Determine whether this driver matches against the given list.
        virtual bool test(const std::span<libdriver::DeviceMatch *> &matches, const bool oand = false);

        /// Instantiates the driver with the given aux data.
        virtual void start(const std::span<std::byte> &auxData);

        /// Return the path to the driver binary.
        const std::string &getPath() const {
            return this->path;
        }

    protected:
        /// Driver binary path
        std::string path;

        /**
         * An array of match structures, each of which describes a device that we can operate on.
         */
        std::vector<libdriver::DeviceMatch *> matches;

        /// when set, the driver does not share tasks
        bool alwaysLaunchNew = false;
        /// handles to all tasks owned by the driver
        std::vector<uintptr_t> taskHandles;
};

#endif
