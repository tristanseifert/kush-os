#ifndef DB_DRIVERDB_H
#define DB_DRIVERDB_H

#include <cassert>
#include <memory>
#include <unordered_map>
#include <string>
#include <shared_mutex>
#include <span>

class Driver;

/**
 * Maintains a repository of all drivers in the system and a;;pws qieruomg for the correct drivers
 * to load for a particular device.
 */
class DriverDb {
    public:
        static void init() {
            assert(!gShared);
            gShared = new DriverDb;
        }

        /// Global driver db instance
        static DriverDb * _Nonnull the() {
            return gShared;
        }

        /// Finds a driver that matches the given match objects.
        //std::shared_ptr<Driver> findDriver(const std::span<libdriver::DeviceMatch *> &matches);

        /// Registers a new driver.
        uintptr_t addDriver(std::shared_ptr<Driver> &driver);
        /// Removes an existing driver.
        bool removeDriver(const uintptr_t id);

    private:
        DriverDb();

    private:
        static DriverDb * _Nonnull gShared;

    private:
        /// Lock over the drivers list
        std::shared_mutex driversLock;
        /// all registered drivers
        std::unordered_map<uintptr_t, std::shared_ptr<Driver>> drivers;
        /// ID for the next driver
        uintptr_t nextId = 1;
};

#endif
