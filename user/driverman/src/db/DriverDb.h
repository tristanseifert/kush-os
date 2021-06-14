#ifndef DB_DRIVERDB_H
#define DB_DRIVERDB_H

#include <cassert>
#include <compare>
#include <memory>
#include <unordered_map>
#include <utility>
#include <string>
#include <string_view>
#include <shared_mutex>
#include <span>

class Device;
class Driver;

/**
 * Maintains a repository of all drivers in the system and a;;pws qieruomg for the correct drivers
 * to load for a particular device.
 */
class DriverDb {
    /// Filesystem path to the early boot 
    constexpr static const std::string_view kBootDbPath{"/config/DriverDb.toml"};

    public:
        /**
         * When matching drivers to devices, more than one driver may match; this structure
         * contains info on this.
         */
        struct MatchInfo {
            std::shared_ptr<Driver> driver;
            int score{0};

            // compare by score
            inline auto operator<=>(const MatchInfo &x) const {
                return this->score <=> x.score;
            }

            MatchInfo() = default;
            MatchInfo(const std::shared_ptr<Driver> &_driver, const int _score) :
                driver(_driver), score(_score) {};
        };

    public:
        static void init() {
            assert(!gShared);
            gShared = new DriverDb;
        }

        /// Global driver db instance
        static DriverDb * _Nonnull the() {
            return gShared;
        }

        /// Find a driver that matches the device.
        std::shared_ptr<Driver> findDriver(const std::shared_ptr<Device> &device,
                MatchInfo * _Nullable driver = nullptr);

        /// Registers a new driver.
        uintptr_t addDriver(const std::shared_ptr<Driver> &driver);
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
